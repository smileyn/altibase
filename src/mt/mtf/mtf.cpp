/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: mtf.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtv.h>
#include <mtuProperty.h>
#include <mtl.h>

#if defined(SMALL_FOOTPRINT)
#define MTF_COLUMN_MAXIMUM (64)
#else
#define MTF_COLUMN_MAXIMUM (1024)
#endif
extern mtdModule mtdBinary;
extern mtdModule mtdSmallint;
extern mtdModule mtdInteger;
extern mtdModule mtdBigint;

extern mtdModule mtdChar;
extern mtdModule mtdVarchar;
extern mtdModule mtdEchar;
extern mtdModule mtdEvarchar;

extern mtdModule mtdFloat;
extern mtdModule mtdNumeric;
extern mtdModule mtdNumber;

extern mtdModule mtdBoolean;
extern mtdModule mtdByte;
extern mtdModule mtdVarbyte;
extern mtdModule mtdNibble;
extern mtdModule mtdBit;
extern mtdModule mtdVarbit;
extern mtdModule mtdNull;
extern mtdModule mtdUndef;

extern mtlModule mtlAscii;
extern mtlModule mtlUTF16;


mtfModule ** mtf::mExternalModule = NULL;

ULong mtf::saveCost[3][3];

UInt mtfNumberOfModulesByName;

mtfNameIndex* mtfModulesByName;

const UInt mtf::comparisonGroup[MTD_GROUP_MAXIMUM][MTD_GROUP_MAXIMUM]={
    /* MISC     MISC     */ { MTD_GROUP_MISC,
    /* MISC     TEXT     */   MTD_GROUP_TEXT,
    /* MISC     NUMBER   */   MTD_GROUP_NUMBER,
    /* MISC     DATE     */   MTD_GROUP_DATE,
    /* MISC     INTERVAL */   MTD_GROUP_INTERVAL },
    /* TEXT     MISC     */ { MTD_GROUP_TEXT,
    /* TEXT     TEXT     */   MTD_GROUP_TEXT,
    /* TEXT     NUMBER   */   MTD_GROUP_NUMBER,
    /* TEXT     DATE     */   MTD_GROUP_DATE,
    /* TEXT     INTERVAL */   MTD_GROUP_INTERVAL },
    /* NUMBER   MISC     */ { MTD_GROUP_NUMBER,
    /* NUMBER   TEXT     */   MTD_GROUP_NUMBER,
    /* NUMBER   NUMBER   */   MTD_GROUP_NUMBER,
    /* NUMBER   DATE     */   MTD_GROUP_MAXIMUM,
    /* NUMBER   INTERVAL */   MTD_GROUP_INTERVAL },
    /* DATE     MISC     */ { MTD_GROUP_DATE,
    /* DATE     TEXT     */   MTD_GROUP_DATE,
    /* DATE     NUMBER   */   MTD_GROUP_MAXIMUM,
    /* DATE     DATE     */   MTD_GROUP_DATE,
    /* DATE     INTERVAL */   MTD_GROUP_MAXIMUM  },
    /* INTERVAL MISC     */ { MTD_GROUP_INTERVAL,
    /* INTERVAL TEXT     */   MTD_GROUP_INTERVAL,
    /* INTERVAL NUMBER   */   MTD_GROUP_INTERVAL,
    /* INTERVAL DATE     */   MTD_GROUP_MAXIMUM,
    /* INTERVAL INTERVAL */   MTD_GROUP_INTERVAL }
};

const mtdModule*** mtf::comparisonTable;

const mtdBooleanType mtf::andMatrix[3][3] = {
    { MTD_BOOLEAN_TRUE,  MTD_BOOLEAN_FALSE, MTD_BOOLEAN_NULL  },
    { MTD_BOOLEAN_FALSE, MTD_BOOLEAN_FALSE, MTD_BOOLEAN_FALSE },
    { MTD_BOOLEAN_NULL,  MTD_BOOLEAN_FALSE, MTD_BOOLEAN_NULL  }
};

const mtdBooleanType mtf::orMatrix[3][3] = {
    { MTD_BOOLEAN_TRUE,  MTD_BOOLEAN_TRUE,  MTD_BOOLEAN_TRUE  },
    { MTD_BOOLEAN_TRUE,  MTD_BOOLEAN_FALSE, MTD_BOOLEAN_NULL  },
    { MTD_BOOLEAN_TRUE,  MTD_BOOLEAN_NULL,  MTD_BOOLEAN_NULL  }
};

// PROJ-2527 WITHIN GROUP AGGR
iduMemPool mtf::mFuncMemoryPool;

static int mtfCompareByName( const mtfNameIndex* aIndex1,
                             const mtfNameIndex* aIndex2 )
{
    return idlOS::strCompare( aIndex1->name->string,
                              aIndex1->name->length,
                              aIndex2->name->string,
                              aIndex2->name->length );
}

IDE_RC mtf::alloc( void* aInfo,
                   UInt  aSize,
                   void** aMemPtr)
{
    return ((mtcCallBackInfo*)aInfo)->memory->alloc( aSize, aMemPtr);
}

IDE_RC mtf::initConversionNodeForInitialize( mtcNode** aConversionNode,
                                             mtcNode*  aNode,
                                             void*     aInfo )
{
/***********************************************************************
 *
 * Description :
 *    Conversion Node�� �����ϰ� �ʱ�ȭ��
 *    mtcCallBack.initConversionNode �� �Լ� �����Ϳ� ���õ�.
 *
 * Implementation :
 *
 *    Conversion Node�� ���� ������ �Ҵ�ް�, ��� Node�� ������.
 *
 ***********************************************************************/
    
    mtcCallBackInfo* sCallBackInfo;
    
    sCallBackInfo = (mtcCallBackInfo*)aInfo;
    
    IDE_TEST(sCallBackInfo->memory->alloc(ID_SIZEOF(mtcNode),
                                          (void**)aConversionNode)
             != IDE_SUCCESS);
    
    idlOS::memcpy(*aConversionNode, aNode, ID_SIZEOF(mtcNode));

    // PROJ-1362
    (*aConversionNode)->baseTable = aNode->baseTable;
    (*aConversionNode)->baseColumn = aNode->baseColumn;
    
    IDE_TEST_RAISE( sCallBackInfo->columnCount >= sCallBackInfo->columnMaximum,
                    ERR_COLUMN_EXCEED );
    
    (*aConversionNode)->column = sCallBackInfo->columnCount;
    
    sCallBackInfo->columnCount++;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_COLUMN_EXCEED );
    IDE_SET(ideSetErrorCode(mtERR_FATAL_COLUMN_EXCEED));

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtf::makeConversionNode( mtcNode**        aConversionNode,
                                mtcNode*         aNode,
                                mtcTemplate*     aTemplate,
                                mtcStack*        aStack,
                                mtcCallBack*     aCallBack,
                                const mtvTable*  aTable )
{
    mtcNode* sNode;
    mtcNode* sLast;
    UInt     sCount;
    mtcStack sStack[2];
    
    if( aTable->count > 0 )
    {
        sStack[0].column = aStack->column;
        for( sCount = 0, sLast = NULL;
             sCount < aTable->count;
             sCount++ )
        {
            sStack[1].column = sStack[0].column;
            IDE_TEST( aCallBack->initConversionNode( &sNode,
                                                     aNode,
                                                     aCallBack->info )
                      != IDE_SUCCESS );
            IDE_TEST( aTable->modules[sCount]->estimate( sNode,
                                                         aTemplate,
                                                         sStack,
                                                         2,
                                                         aCallBack )
                      != IDE_SUCCESS );
            
            sNode->conversion = sLast;
            sNode->lflag      = 1;
            sLast             = sNode;
        }
        *aConversionNode = sLast;
        aStack->column = sStack[0].column;
    }
    else
    {
        *aConversionNode = NULL;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtf::convertInternal( mtcNode*     aNode,
                             mtcStack*    aStack,
                             mtcTemplate* aTemplate )
{
    if( aNode->conversion != NULL )
    {
        IDE_TEST( convertInternal(                          aNode->conversion,
                                                                       aStack,
                                                                    aTemplate )
                  != IDE_SUCCESS );
    }
    
    aStack[1]        = aStack[0];

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;
    aStack[0].value  = (UChar*)
        mtd::valueForModule( (smiColumn*) aStack[0].column,
                             aTemplate->rows[aNode->table].row,
                             MTD_OFFSET_USE,
                             aStack[0].column->module->staticNull );
    
    // BUG-34321 return value optimization
    return aTemplate->rows[aNode->table].
        execute[aNode->column].calculate(
            aNode,
            aStack,
            0,
            aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo,
            aTemplate );
    
    //return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtf::initializeComparisonTable( void )
{
    UInt             sStage = 0;
    UInt             sArgument1;
    UInt             sArgument2;
    UInt             sTarget;
    ULong            sCost;
    const mtdModule* sModule1;
    const mtdModule* sModule2;
    const mtdModule* sModule;
    const mtvTable*  sTable1;
    const mtvTable*  sTable2;

    IDE_TEST(iduMemMgr::malloc(IDU_MEM_MT,
                               ID_SIZEOF(mtdModule**) *
                               mtd::getNumberOfModules(),
                               (void**)&comparisonTable)
             != IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(iduMemMgr::malloc(IDU_MEM_MT,
                               ID_SIZEOF(mtdModule*) *
                               mtd::getNumberOfModules() *
                               mtd::getNumberOfModules(),
                               (void**)&(comparisonTable[0]))
             != IDE_SUCCESS);
    sStage = 2;
    
    for( sArgument1 = 0; sArgument1 < mtd::getNumberOfModules(); sArgument1++ )
    {
        comparisonTable[sArgument1] =
                  comparisonTable[0] + sArgument1 * mtd::getNumberOfModules();
    }
    
    for( sArgument1 = 0;
         sArgument1 < mtd::getNumberOfModules();
         sArgument1++ )
    {
        IDE_TEST( mtd::moduleByNo( &sModule1, sArgument1 )
                  != IDE_SUCCESS );
        
        for( sArgument2 = 0;
             sArgument2 < mtd::getNumberOfModules();
             sArgument2++ )
        {
            IDE_TEST( mtd::moduleByNo( &sModule2, sArgument2 )
                      != IDE_SUCCESS );
            
            comparisonTable[sArgument1][sArgument2] = NULL;
            
            for( sTarget = 0, sCost = MTV_COST_INFINITE;
                 sTarget < mtd::getNumberOfModules();
                 sTarget++ )
            {
                IDE_TEST( mtd::moduleByNo( &sModule, sTarget )
                          != IDE_SUCCESS );
                
                if( ( sModule->flag & MTD_GROUP_MASK ) ==
                    comparisonGroup[sModule1->flag&MTD_GROUP_MASK]
                                   [sModule2->flag&MTD_GROUP_MASK] )
                {
                    IDE_TEST( mtv::tableByNo( &sTable1, sTarget, sArgument1 )
                              != IDE_SUCCESS );
                    IDE_TEST( mtv::tableByNo( &sTable2, sTarget, sArgument2 )
                              != IDE_SUCCESS );
                    
                    if( sCost > sTable1->cost + sTable2->cost )
                    {
                        sCost = sTable1->cost + sTable2->cost;
                        IDE_TEST(
                            mtd::moduleByNo(
                                &comparisonTable[sArgument1][sArgument2],
                                sTarget )
                            != IDE_SUCCESS );
                    }
                }
            }
        }
    }

    /* PATCH TABLE : CHANGE FLOAT ASSOCIATED WITH NUMERIC TO NUMERIC */
    for( sArgument1 = 0;
         sArgument1 < mtd::getNumberOfModules();
         sArgument1++ )
    {
        if( comparisonTable[mtdNumeric.no][sArgument1] == &mtdFloat )
        {
            comparisonTable[mtdNumeric.no][sArgument1] = &mtdNumeric;
        }
        if( comparisonTable[sArgument1][mtdNumeric.no] == &mtdFloat )
        {
            comparisonTable[sArgument1][mtdNumeric.no] = &mtdNumeric;
        }
    }
    /* PATCH TABLE : CHANGE TO BYTE
                     ASSOCIATED WITH BYTE AND TEXT */
    for( sArgument1 = 0;
         sArgument1 < mtd::getNumberOfModules();
         sArgument1++ )
    {
        IDE_TEST( mtd::moduleByNo( &sModule1, sArgument1 )
                  != IDE_SUCCESS );
        
        for( sArgument2 = 0;
             sArgument2 < mtd::getNumberOfModules();
             sArgument2++ )
        {
            IDE_TEST( mtd::moduleByNo( &sModule2, sArgument2 )
                      != IDE_SUCCESS );
            
            if( ( ( (sModule1->flag & MTD_GROUP_MASK) == MTD_GROUP_TEXT ) &&
                  ( sModule2 == &mtdByte ) )
                ||
                ( ( (sModule2->flag & MTD_GROUP_MASK) == MTD_GROUP_TEXT ) &&
                  ( sModule1 == &mtdByte ) ) )
            {
                comparisonTable[sArgument1][sArgument2] = &mtdByte;
            }
            
            if( ( ( (sModule1->flag & MTD_GROUP_MASK) == MTD_GROUP_TEXT ) &&
                  ( sModule2 == &mtdVarbyte ) )
                ||
                ( ( (sModule2->flag & MTD_GROUP_MASK) == MTD_GROUP_TEXT ) &&
                  ( sModule1 == &mtdVarbyte ) ) )
            {
                comparisonTable[sArgument1][sArgument2] = &mtdVarbyte;
            }
        }
    }
    /* BUG-40539
     * PATCH TABLE : undef type�� ���ϴ� ��� �׻� undef type���� ����
     */
    for ( sArgument1 = 0;
          sArgument1 < mtd::getNumberOfModules();
          sArgument1++ )
    {
        comparisonTable[mtdUndef.no][sArgument1] = &mtdUndef;
        comparisonTable[sArgument1][mtdUndef.no] = &mtdUndef;
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    switch( sStage )
    {
     case 2:
         (void)iduMemMgr::free(comparisonTable[0]);
         comparisonTable[0] = NULL;
         
     case 1:
         (void)iduMemMgr::free(comparisonTable);
         comparisonTable = NULL;
     default:
        break;
    }
    
    return IDE_FAILURE;
}

IDE_RC mtf::finalizeComparisonTable( void )
{
    if( comparisonTable != NULL )
    {
        IDE_TEST(iduMemMgr::free( comparisonTable[0] )
                 != IDE_SUCCESS);
        comparisonTable[0] = NULL;
        
        IDE_TEST(iduMemMgr::free( comparisonTable ) != IDE_SUCCESS);
        comparisonTable = NULL;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

mtcNode* mtf::convertedNode( mtcNode*      aNode,
                             mtcTemplate*  /* aTemplate */)
{
/***********************************************************************
 *
 * Description : conversion node ��ȯ 
 *               
 *
 * Implementation :
 *    conversion, indirect conversion�� �����ؼ�
 *    ���� conversion node�� ��ȯ 
 *
 ***********************************************************************/  
    mtcNode* sNode;
    mtcNode* sConversion;

    for( sNode = aNode;
         ( sNode->lflag & MTC_NODE_INDIRECT_MASK ) == MTC_NODE_INDIRECT_TRUE;
         sNode = sNode->arguments ) ;

    sConversion = sNode->conversion;

    // PROJ-2163 : Remove indirect conversion node
    if ( sConversion != NULL )
    {
        sNode = sConversion;
    }

    return sNode;
}

IDE_RC mtf::postfixCalculate( mtcNode*     aNode,
                              mtcStack*    aStack,
                              SInt         aRemain,
                              void*,
                              mtcTemplate* aTemplate )
{
    mtcNode*  sNode;
    mtcStack* sStack;
    SInt      sRemain;
    
    IDE_TEST_RAISE( aRemain < 1, ERR_STACK_OVERFLOW );

    aStack->column = aTemplate->rows[aNode->table].columns + aNode->column;
    aStack->value  = (void*)
        mtd::valueForModule( (smiColumn*) aStack->column,
                             aTemplate->rows[aNode->table].row,
                             MTD_OFFSET_USE,
                             aStack->column->module->staticNull );
    
    for( sNode  = aNode->arguments, sStack = aStack + 1, sRemain = aRemain - 1;
         sNode != NULL;
         sNode  = sNode->next, sStack++, sRemain-- )
    {
        // BUG-33674
        IDE_TEST_RAISE( sRemain < 1, ERR_STACK_OVERFLOW );
        
        if (sNode->column != MTC_RID_COLUMN_ID)
        {
            IDE_TEST(aTemplate->rows[sNode->table].
                     execute[sNode->column].calculate(
                         sNode,
                         sStack,
                         sRemain,
                         aTemplate->rows[sNode->table].
                         execute[sNode->column].calculateInfo,
                         aTemplate)
                     != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(aTemplate->rows[sNode->table].
                     ridExecute->calculate(sNode,
                                           sStack,
                                           sRemain,
                                           NULL,
                                           aTemplate)
                     != IDE_SUCCESS);
        }

        if( sNode->conversion != NULL )
        {
            IDE_TEST( convertCalculate( sNode, 
                                        sStack,
                                        sRemain,
                                        NULL,
                                        aTemplate )
                      != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtf::convertCalculate( mtcNode*     aNode,
                              mtcStack*    aStack,
                              SInt,
                              void*,
                              mtcTemplate* aTemplate )
{
    mtcNode* sNode;
    mtcStack sStack[2];

    sNode = aNode->conversion;

    // PROJ-2163 : Remove indirect conversion node
    if( sNode != NULL )
    {
        sStack[0] = aStack[0];
        IDE_TEST( convertInternal( sNode,
                                   sStack,
                                   aTemplate )
                  != IDE_SUCCESS );
        aStack[0] = sStack[0];
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtf::convertLeftCalculate( mtcNode*     aNode,
                                  mtcStack*    aStack,
                                  SInt,
                                  void*,
                                  mtcTemplate* aTemplate )
{
    mtcNode* sNode;
    mtcStack sStack[2];

    // fix BUG-32079
    // indirect node can not have left conversion node.
    for( ;
         ( aNode->lflag & MTC_NODE_INDIRECT_MASK ) == MTC_NODE_INDIRECT_TRUE;
         aNode = aNode->arguments ) ;

    sNode = aNode->leftConversion;

    // PROJ-2163 : Remove indirect conversion node
    if( sNode != NULL )
    {
        sStack[0] = aStack[0];
        IDE_TEST( convertInternal( sNode,
                                    sStack,
                                    aTemplate )
                    != IDE_SUCCESS );
        aStack[0] = sStack[0];
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtf::initializeDefault( void )
{
    return IDE_SUCCESS;
}

IDE_RC mtf::finalizeDefault( void )
{
    return IDE_SUCCESS;
}

IDE_RC mtf::estimateNA( mtcNode*,
                        mtcTemplate*,
                        mtcStack*,
                        SInt,
                        mtcCallBack* )
{
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));
    
    return IDE_FAILURE;
}

IDE_RC mtf::calculateNA( mtcNode*,
                         mtcStack*,
                         SInt,
                         void*,
                         mtcTemplate* )
{
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_APPLICABLE));
    
    return IDE_FAILURE;
}

IDE_RC mtf::makeConversionNodes( mtcNode*          aNode,
                                 mtcNode*          aArguments,
                                 mtcTemplate*      aTemplate,
                                 mtcStack*         aStack,
                                 mtcCallBack*      aCallBack,
                                 const mtdModule** aModule )
{
    mtcNode*        sNode;
    mtcNode*        sArguments;
    mtcNode*        sConversionNode;
    const mtvTable* sTable;
    ULong           sCost;

    sCost = 0;

    for( sArguments  = aArguments;
         sArguments != NULL;
         sArguments  = sArguments->next, aStack++, aModule++ )
    {
        for( sNode = sArguments;
             ( sNode->lflag & MTC_NODE_INDIRECT_MASK ) == MTC_NODE_INDIRECT_TRUE;
             sNode = sNode->arguments ) ;

        if( ( ( (*aModule)->id >= MTD_UDT_ID_MIN ) &&
              ( (*aModule)->id <= MTD_UDT_ID_MAX ) )  ||
            ( (*aModule)->id == MTD_UNDEF_ID ) )
        {
            // To fix BUG-15116
            // UDT�� ���ؼ��� conversion node�� �� �� ����.

            // PROJ-2163 : Undef Ÿ�Կ� ���ؼ��� conversion node �� �� �� ����.
            continue;
        }
        else
        {
            // BUG-43858 ���ڰ� Undef Ÿ���� ��� aNode�� ǥ���Ѵ�.
            if ( aStack->column->module->id == MTD_UNDEF_ID )
            {
                aNode->lflag &= ~MTC_NODE_UNDEF_TYPE_MASK;
                aNode->lflag |= MTC_NODE_UNDEF_TYPE_EXIST;
            }
            else
            {
                // Nothing to do.
            }

            IDE_TEST( mtv::tableByNo( &sTable,
                                      (*aModule)->no,
                                      aStack->column->module->no )
                      != IDE_SUCCESS );

            sCost += sTable->cost;

            IDE_TEST_RAISE( sCost >= MTV_COST_INFINITE, ERR_CONVERT );

            IDE_TEST( makeConversionNode( &sConversionNode,
                                          sNode,
                                          aTemplate,
                                          aStack,
                                          aCallBack,
                                          sTable )
                      != IDE_SUCCESS );

            if( sNode->conversion == NULL )
            {
                sNode->conversion = sConversionNode;
                aNode->cost = sCost;
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtf::makeLeftConversionNodes( mtcNode*          aNode,
                                     mtcNode*          aArguments,
                                     mtcTemplate*      aTemplate,
                                     mtcStack*         aStack,
                                     mtcCallBack*      aCallBack,
                                     const mtdModule** aModule )
{
    mtcNode*        sNode;
    mtcNode*        sArguments;
    mtcNode*        sConversionNode;
    const mtvTable* sTable;
    ULong           sCost;

    sCost = 0;

    for( sArguments  = aArguments;
         sArguments != NULL;
         sArguments  = sArguments->next, aStack++, aModule++ )
    {
        // fix BUG-32079
        // one column subquery node has MTC_NODE_INDIRECT_TRUE flag.
        // ex) 
        // var a integer;
        // create table a ( dummy char(1) primary key );
        // prepare select 'a' from a where dummy in ( :a, (select dummy from a) );
        for( sNode = sArguments;
             ( sNode->lflag & MTC_NODE_INDIRECT_MASK ) == MTC_NODE_INDIRECT_TRUE;
             sNode = sNode->arguments ) ;

        // PROJ-2163
        if( (*aModule)->id == MTD_UNDEF_ID )
        {
            // undef Ÿ�Կ� ���ؼ��� conversion node�� �� �� ����.
            continue;
        }
        else
        {
            // BUG-43858 ���ڰ� Undef Ÿ���� ��� aNode�� ǥ���Ѵ�.
            if ( aStack->column->module->id == MTD_UNDEF_ID )
            {
                aNode->lflag &= ~MTC_NODE_UNDEF_TYPE_MASK;
                aNode->lflag |= MTC_NODE_UNDEF_TYPE_EXIST;
            }
            else
            {
                // Nothing to do.
            }

            IDE_TEST( mtv::tableByNo( &sTable,
                      (*aModule)->no,
                      aStack->column->module->no )
                    != IDE_SUCCESS );

            sCost += sTable->cost;

            IDE_TEST_RAISE( sCost > MTV_COST_INFINITE, ERR_CONVERT );

            IDE_TEST( makeConversionNode( &sConversionNode,
                      sNode,
                      aTemplate,
                      aStack,
                      aCallBack,
                      sTable )
                    != IDE_SUCCESS );

            if( sNode->leftConversion == NULL )
            {
                sNode->leftConversion = sConversionNode;
                aNode->cost = sCost;
            }
            else
            {
            // Nothing to do.
            }
        }

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtf::initializeComparisonTemplate(
           mtfSubModule**** aTable,
           mtfSubModule*    aGroupTable[MTD_GROUP_MAXIMUM][MTD_GROUP_MAXIMUM],
           mtfSubModule*    aDefaultModule )
{
    UInt             sStage = 0;
    UInt             sArgument1;
    UInt             sArgument2;
    const mtdModule* sModule1;
    const mtdModule* sModule2;
    mtfSubModule***  sTable;
    mtfSubModule*    sSubModule;
    ULong            sCost;
    mtcNode          sNode[3];
    mtcTemplate      sTemplate;
    mtcTuple         sTuple;
    mtcColumn        sColumns[MTF_COLUMN_MAXIMUM];
    mtcExecute       sExecute[MTF_COLUMN_MAXIMUM];
    mtcStack         sStack[MTF_COLUMN_MAXIMUM*2];
    mtcCallBackInfo  sCallBackInfo;
    mtcCallBack      sCallBack;
    iduMemory        sMemory;

    sMemory.init(IDU_MEM_MT);
    IDE_TEST(iduMemMgr::malloc(IDU_MEM_MT,
                               ID_SIZEOF(mtfSubModule**) *
                               mtd::getNumberOfModules(),
                               (void**)&sTable)
             != IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(iduMemMgr::malloc(IDU_MEM_MT,
                               ID_SIZEOF(mtfSubModule*)  *
                               mtd::getNumberOfModules() *
                               mtd::getNumberOfModules(),
                               (void**)&(sTable[0])) != IDE_SUCCESS);
    sStage = 2;

    for( sArgument1 = 0;
         sArgument1 < mtd::getNumberOfModules();
         sArgument1++ )
    {
        sTable[sArgument1] = sTable[0]
                           + mtd::getNumberOfModules() * sArgument1;
    }

    sNode[0].arguments = sNode + 1;
    sNode[0].next      = NULL;
    sNode[0].table     = 0;
    sNode[0].column    = 0;
    sNode[0].lflag     = 2;
    sNode[0].module    = NULL;

    sNode[1].next      = sNode + 2;
    sNode[1].table     = 0;
    sNode[1].column    = 1;
    sNode[1].lflag     = 0;
    sNode[1].module    = NULL;

    sNode[2].next      = NULL;
    sNode[2].table     = 0;
    sNode[2].column    = 2;
    sNode[2].lflag     = 0;
    sNode[2].module    = NULL;

    // PROJ-1358
    // estimate() �Լ��� interface ��������
    // ������ Template �� �����Ͽ� �ʱ�ȭ�� ����Ѵ�.
    sTemplate.rows = & sTuple;
    sTemplate.rowArrayCount = 1;
    sTemplate.rowCount = 0;

    sTuple.columns  = sColumns;
    sTuple.execute  = sExecute;

    sCallBackInfo.memory         = &sMemory;
    sCallBackInfo.columnMaximum  = MTF_COLUMN_MAXIMUM;

    sCallBack.info               = &sCallBackInfo;
    sCallBack.flag               = MTC_ESTIMATE_INITIALIZE_TRUE;
    sCallBack.alloc              = mtf::alloc;

    sCallBack.initConversionNode         = mtf::initConversionNodeForInitialize;

    for( sArgument1 = 0;
         sArgument1 < mtd::getNumberOfModules();
         sArgument1++ )
    {
        IDE_TEST( mtd::moduleByNo( &sModule1, sArgument1 ) != IDE_SUCCESS );
        if( sModule1->column != NULL )
        {
            sTuple.columns[sNode[1].column] = *(sModule1->column);
        }
        for( sArgument2 = 0;
             sArgument2 < mtd::getNumberOfModules();
             sArgument2++ )
        {
            IDE_TEST( mtd::moduleByNo( &sModule2, sArgument2 )
                      != IDE_SUCCESS );

            if( sModule2->column != NULL )
            {
                sTuple.columns[sNode[2].column] = *(sModule2->column);
            }

            sTable[sArgument1][sArgument2]  = aDefaultModule;
            sCost                           = MTV_COST_INFINITE;

            if( sModule1->column != NULL && sModule2->column != NULL )
            {
                for( sSubModule  = aGroupTable[sModule1->flag&MTD_GROUP_MASK]
                                              [sModule2->flag&MTD_GROUP_MASK];
                     sSubModule != NULL;
                     sSubModule  = sSubModule->next )
                {
                    sCallBackInfo.columnCount = 3;
                    sNode[0].cost             = 0;
                    sNode[1].conversion       = NULL;
                    sNode[2].conversion       = NULL;
                    sStack[0].column          = sColumns + sNode[0].column;
                    sStack[1].column          = sColumns + sNode[1].column;
                    sStack[2].column          = sColumns + sNode[2].column;
                    if( sSubModule->estimate( &sNode[0],
                                              &sTemplate,
                                              sStack,
                                              ID_SIZEOF(sStack)/
                                              ID_SIZEOF(mtcStack),
                                              &sCallBack )
                        == IDE_SUCCESS )
                    {
                        if( sNode[0].cost < sCost )
                        {
                            sCost                          = sNode[0].cost;
                            sTable[sArgument1][sArgument2] = sSubModule;
                        }
                    }
                    sCallBackInfo.memory->clear();
                }
            }
        }
    }
    
    *aTable = sTable;

//    sMemory.destroy();
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    switch( sStage )
    {
        case 2:
         (void)iduMemMgr::free(sTable[0]);
         sTable[0] = NULL;
         
        case 1:
         (void)iduMemMgr::free(sTable);
         sTable = NULL;

        default:
            break;
    }

    sMemory.destroy();
    return IDE_FAILURE;
}

IDE_RC mtf::finalizeComparisonTemplate( mtfSubModule**** aTable )
{
    if( *aTable != NULL )
    {
        IDE_TEST(iduMemMgr::free( (*aTable)[0]) != IDE_SUCCESS);
        (*aTable)[0] = NULL;

        IDE_TEST(iduMemMgr::free( *aTable ) != IDE_SUCCESS);
        *aTable = NULL;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC mtf::initializeTemplate( mtfSubModule*** aTable,
                                mtfSubModule*   aEstimates,
                                mtfSubModule*   aDefaultModule )
{
    UInt             sStage = 0;
    UInt             sArgument;
    const mtdModule* sModule;
    mtfSubModule**   sTable;
    mtfSubModule*    sSubModule;
    ULong            sCost;
    mtcNode          sNode[2];
    mtcTemplate      sTemplate;
    mtcTuple         sTuple;
    mtcColumn        sColumns[MTF_COLUMN_MAXIMUM*2];
    mtcExecute       sExecute[MTF_COLUMN_MAXIMUM];
    mtcStack         sStack[MTF_COLUMN_MAXIMUM*2];
    mtcCallBackInfo  sCallBackInfo;
    mtcCallBack      sCallBack;
    iduMemory        sMemory;

    sMemory.init(IDU_MEM_MT);
    IDE_TEST(iduMemMgr::malloc(IDU_MEM_MT,
                               ID_SIZEOF(mtfSubModule*) *
                               mtd::getNumberOfModules(),
                               (void**)&sTable)
             != IDE_SUCCESS);
    sStage = 1;
    
    sNode[0].arguments = sNode + 1;
    sNode[0].next      = NULL;
    sNode[0].table     = 0;
    sNode[0].column    = 0;
    sNode[0].lflag     = 1;
    sNode[0].module    = NULL;
 
    sNode[1].next      = NULL;
    sNode[1].table     = 0;
    sNode[1].column    = MTF_COLUMN_MAXIMUM;
    sNode[1].lflag     = 0;
    sNode[1].module    = NULL;

    sTemplate.rows = & sTuple;
    sTemplate.rowArrayCount = 1;
    sTemplate.rowCount = 0;
    
    sTuple.columns = sColumns;
    sTuple.execute = sExecute;
    
    sCallBackInfo.memory         = &sMemory;
    sCallBackInfo.columnMaximum  = MTF_COLUMN_MAXIMUM;
    
    sCallBack.info               = &sCallBackInfo;
    sCallBack.flag               = MTC_ESTIMATE_INITIALIZE_TRUE;
    sCallBack.alloc              = mtf::alloc;

    sCallBack.initConversionNode         = mtf::initConversionNodeForInitialize;

    for( sArgument = 0;
         sArgument < mtd::getNumberOfModules();
         sArgument++ )
    {
        IDE_TEST( mtd::moduleByNo( &sModule, sArgument ) != IDE_SUCCESS );
        sTable[sArgument]               = aDefaultModule;
        sCost                           = MTV_COST_INFINITE;
        if( sModule->column != NULL )
        {
            sTuple.columns[sNode[1].column] = *(sModule->column);
            for( sSubModule  = aEstimates;
                 sSubModule != NULL;
                 sSubModule  = sSubModule->next )
            {
                sCallBackInfo.columnCount = 2;
                sNode[0].cost             = 0;
                sNode[1].conversion       = NULL;
                sStack[0].column          = sColumns + sNode[0].column;
                sStack[1].column          = sColumns + sNode[1].column;
                if( sSubModule->estimate( &sNode[0],
                                          &sTemplate,
                                          sStack,
                                          ID_SIZEOF(sStack)/ID_SIZEOF(mtcStack),
                                          &sCallBack )
                    == IDE_SUCCESS )
                {
                    if( sNode[0].cost < sCost )
                    {
                        sCost              = sNode[0].cost;
                        sTable[sArgument] = sSubModule;
                    }
                }
                sCallBackInfo.memory->clear();
            }
        }
    }
    
    *aTable = sTable;

//    sMemory.destroy();
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    switch( sStage )
    {
     case 1:
         (void)iduMemMgr::free(sTable);
         sTable = NULL;
         
     default:
        break;
    }

    sMemory.destroy();
    return IDE_FAILURE;
}

IDE_RC mtf::finalizeTemplate( mtfSubModule*** aTable )
{
    if( *aTable != NULL )
    {
        IDE_TEST(iduMemMgr::free( *aTable ) != IDE_SUCCESS);
        
        *aTable = NULL;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC mtf::getCharFuncResultModule( const mtdModule** aResultModule,
                                     const mtdModule*  aArgumentModule )
{
    // ���� ���� �Լ�(upper, lower, concat...)��
    // ����� Ÿ���� ������ Ÿ���� �������� �Ѵ�.
    // ��, upper(i1)���� i1�� CHAR Ÿ���̸�
    // ����� CHAR�̰� i1�� VARCHAR�̸� ����� VARCHAR�̴�.
    // �̴� ����Ŭ�� ���� ��å�̴�.
    //
    // by kumdory, 2005-06-21    

    /***********************************************************************
     *  // PROJ-2002 Column Security
     *
     *  ECHAR        => CHAR ��ȯ
     *  ECHAR_ECC    => CHAR ��ȯ
     *  EVARCHAR     => VARCHAR ��ȯ
     *  EVARCHAR_ECC => VARCHAR ��ȯ
     *
     ***********************************************************************/
    
    if( aArgumentModule == NULL )
    {
        IDE_TEST( mtd::moduleByName( aResultModule, "VARCHAR", 7 )
                  != IDE_SUCCESS );
    }
    else if( aArgumentModule->id == MTD_CHAR_ID )
    {
        *aResultModule = aArgumentModule;
    }
    else if( aArgumentModule->id == MTD_NCHAR_ID )
    {
        *aResultModule = aArgumentModule;
    }
    else if( aArgumentModule->id == MTD_NVARCHAR_ID )
    {
        *aResultModule = aArgumentModule;
    }
    // PROJ-2002 Column Security
    else if( aArgumentModule->id == MTD_ECHAR_ID ) 
    {
        IDE_TEST( mtd::moduleByName( aResultModule, "CHAR", 4 )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mtd::moduleByName( aResultModule, "VARCHAR", 7 )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtf::getLobFuncResultModule( const mtdModule** aResultModule,
                                    const mtdModule*  aArgumentModule )
{
    if( aArgumentModule == NULL )
    {
        IDE_TEST( mtd::moduleByName( aResultModule, "BLOB_LOCATOR", 12 )
                  != IDE_SUCCESS );
    }
    else if( aArgumentModule->id == MTD_BLOB_ID )
    {
        IDE_TEST( mtd::moduleByName( aResultModule, "BLOB_LOCATOR", 12 )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mtd::moduleByName( aResultModule, "CLOB_LOCATOR", 12 )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}    

IDE_RC mtf::getLikeModule( const mtdModule** aResultModule,
                           const mtdModule*  aSearchValueModule,
                           const mtdModule*  /*aPatternModule*/ )
{
    // BUG-11194 fix
    // like �Լ��� datatype ��å
    //
    // char1 like char2 ( escape esc_char )
    //
    //   - char1   : search value
    //   - char2   : pattern
    //   - esc_char: escape character
    //
    //   * char1, char2�� CHAR �Ǵ� VARCHAR Ÿ���� �� �ִ�.
    //     ����Ŭ�� ���� char1�� like ����� ����ϰ� �Ѵ�.
    //     ��, char1�� CHAR Ÿ���̸� char_like��,
    //     char1�� VARCHAR Ÿ���̸� varchar_like�� ����ϰ� �ϱ� ����
    //     char1�� Ÿ���� �����ϵ��� �Ѵ�.
    //     ������ char2�� Ÿ�԰� char1�� Ÿ���� �ٸ���,
    //     char2�� Ÿ���� char1�� Ÿ������ conversion�Ǿ� ������,
    //     PROJ-1364�� ���� �׷� �ʿ䰡 ���� �Ǿ���.
    //
    // by kumdory, 2005-06-21

    // BUGBUG language ���� �ڵ�� PROJ-1361���� �������� ��.
    
    if( aSearchValueModule->id == MTD_CHAR_ID ) 
    {
        *aResultModule = aSearchValueModule;
    }
    // PRPJ-1579 NCHAR
    else if( aSearchValueModule->id == MTD_NCHAR_ID )
    {
        *aResultModule = aSearchValueModule;
    }
    else if( aSearchValueModule->id == MTD_NVARCHAR_ID )
    {
        *aResultModule = aSearchValueModule;
    }
    // PROJ-2002 Column Security
    else if( aSearchValueModule->id == MTD_ECHAR_ID ) 
    {
        *aResultModule = aSearchValueModule;
    }
    else if( aSearchValueModule->id == MTD_EVARCHAR_ID ) 
    {
        *aResultModule = aSearchValueModule;
    }
    else
    {
        IDE_TEST( mtd::moduleByName( aResultModule, "VARCHAR", 7 )
                  != IDE_SUCCESS );   
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtf::getCharFuncCharResultModule( const mtdModule** aResultModule,
                                         const mtdModule*  aArgumentModule )
{
/***********************************************************************
 *
 * Description : PROJ-1579 NCHAR
 *
 *      CHAR     => CHAR ��ȯ
 *      NCHAR    => CHAR ��ȯ
 *      VARCHAR  => VARCHAR ��ȯ
 *      NVARCHAR => VARCHAR ��ȯ
 *
 *      ���� �Լ����� NCHAR/NVARCHAR Ÿ���� ���� ó������ �ʰ�,
 *      CHAR/VARCHAR Ÿ������ ��ȯ�ؼ� ������ �ϴ� �Լ��� ���
 *      ȣ��Ǵ� �Լ��̴�.
 *
 *      // PROJ-2002 Column Security
 *      ECHAR        => CHAR ��ȯ
 *      EVARCHAR     => VARCHAR ��ȯ
 *
 * Implementation :
 *
 *
 ***********************************************************************/


    if( aArgumentModule == NULL )
    {
        IDE_TEST( mtd::moduleByName( aResultModule, "VARCHAR", 7 )
                  != IDE_SUCCESS );
    }
    // PROJ-2002 Column Security
    else if( (aArgumentModule->id == MTD_CHAR_ID)  ||
             (aArgumentModule->id == MTD_NCHAR_ID) ||
             (aArgumentModule->id == MTD_ECHAR_ID) )
    {
        *aResultModule = &mtdChar;
    }
    else
    {
        IDE_TEST( mtd::moduleByName( aResultModule, "VARCHAR", 7 )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtf::initialize( mtfModule *** aExtFuncModuleGroup,
                        UInt          aGroupCnt )
{
    UInt              i;
    UInt              sCnt;
    UInt              sExternalCntByName;
    
    UInt              sStage = 0;
    mtfModule**       sModule;
    const mtcName*    sName;
    idBool            sChangeCost = ID_FALSE;

    // BUG-34342
    // ������� ��尡 performance�� ���
    if ( MTU_ARITHMETIC_OP_MODE >= MTC_ARITHMETIC_OPERATION_PERFORMANCE_LEVEL1 )
    {
        // ��ȯ Cost�� �����ϰ� ������ ���� �� Native C Type (double ��)
        // �� ���õ� Ȯ���� ���δ�.
        IDE_TEST( changeConvertCost() != IDE_SUCCESS );
        sChangeCost = ID_TRUE;
    }
    else
    {
        // Nothing to do.
    }
    
    //---------------------------------------------------------------
    // ���� �������� ��ȯ ���̺� ����
    //---------------------------------------------------------------
    
    for( sModule = (mtfModule**) mInternalModule, mtfNumberOfModulesByName = 0;
         *sModule != NULL;
         sModule++ )
    {
        IDE_TEST( (*sModule)->initialize() != IDE_SUCCESS );
        for( sName  = (*sModule)->names; sName != NULL; sName  = sName->next )
        {
            mtfNumberOfModulesByName++;
        }
    }

    if ( sChangeCost == ID_TRUE )
    {
        // �����ߴ� Cost�� ����
        IDE_TEST( restoreConvertCost() != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    //---------------------------------------------------------------
    // �ܺ� �������� ��ȯ ���̺� ����
    //---------------------------------------------------------------

    sExternalCntByName = 0;
    
    for ( i = 0; i < aGroupCnt; i++ )
    {
        for( sModule   = aExtFuncModuleGroup[i];
             *sModule != NULL;
             sModule++ )
        {
            IDE_TEST( (*sModule)->initialize() != IDE_SUCCESS );
            for( sName  = (*sModule)->names;
                 sName != NULL;
                 sName  = sName->next )
            {
                mtfNumberOfModulesByName++;
                sExternalCntByName++;
            }
        }
    }

    // BUG-26712
    IDE_TEST(iduMemMgr::malloc(IDU_MEM_MT,
                               ID_SIZEOF(mtfModule*)
                               * ( sExternalCntByName + 1),
                               (void**) & mExternalModule )
             != IDE_SUCCESS);
    sStage = 1;

    sCnt = 0;
    for ( i = 0; i < aGroupCnt; i++ )
    {
        for (  sModule = aExtFuncModuleGroup[i];
               *sModule != NULL;
               sModule++, sCnt++ )
        {
            mExternalModule[sCnt] = *sModule;
        }
    }
    mExternalModule[sCnt] = NULL;
    
    IDE_TEST(iduMemMgr::malloc(IDU_MEM_MT,
                               ID_SIZEOF(mtfNameIndex) * mtfNumberOfModulesByName,
                               (void**)&mtfModulesByName)
             != IDE_SUCCESS);
    sStage = 2;

    for( sModule = (mtfModule**) mInternalModule, mtfNumberOfModulesByName = 0;
         *sModule != NULL;
         sModule++ )
    {
        for( sName  = (*sModule)->names;
             sName != NULL;
             sName  = sName->next, mtfNumberOfModulesByName++ )
        {
            mtfModulesByName[mtfNumberOfModulesByName].name   = sName;
            mtfModulesByName[mtfNumberOfModulesByName].module = *sModule;
        }
    }

    for( sModule   = mExternalModule;
         *sModule != NULL;
         sModule++ )
    {
        for( sName  = (*sModule)->names;
             sName != NULL;
             sName  = sName->next, mtfNumberOfModulesByName++ )
        {
            mtfModulesByName[mtfNumberOfModulesByName].name   = sName;
            mtfModulesByName[mtfNumberOfModulesByName].module = *sModule;
        }
    }
    
    idlOS::qsort( mtfModulesByName, mtfNumberOfModulesByName,
                  ID_SIZEOF(mtfNameIndex), (PDL_COMPARE_FUNC)mtfCompareByName );

    //---------------------------------------------------------------
    // �� �������� ��ȯ ���̺� ����
    //---------------------------------------------------------------
    
    IDE_TEST( initializeComparisonTable() != IDE_SUCCESS );

    //---------------------------------------------------------------
    // PROJ-2527 WITHIN GROUP AGGR
    // �Լ����� ����� memory pool ����
    //---------------------------------------------------------------
    
    IDE_TEST( mFuncMemoryPool.initialize( IDU_MEM_MT,
                                          (SChar*)"MT_FUNCTION_MEMORY_POOL",
                                          ID_SCALABILITY_SYS,
                                          ID_SIZEOF(iduMemory),
                                          MTF_MEMPOOL_ELEMENT_CNT,
                                          IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkSize */
                                          ID_TRUE,							/* UseMutex */
                                          IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                          ID_FALSE,							/* ForcePooling */
                                          ID_TRUE,							/* GarbageCollection */
                                          ID_TRUE )							/* HWCacheLine */
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    switch( sStage )
    {
     case 2:
         (void)iduMemMgr::free(mtfModulesByName);
         mtfModulesByName = NULL;
     case 1:
         (void)iduMemMgr::free(mExternalModule);
         mExternalModule = NULL;
         
     default:
        break;
    }

    return IDE_FAILURE;
}

IDE_RC mtf::finalize( void )
{
    mtfModule** sModule;
    
    finalizeComparisonTable();

    for( sModule = (mtfModule**) mInternalModule; *sModule != NULL; sModule++ )
    {
        IDE_TEST( (*sModule)->finalize() != IDE_SUCCESS );
    }

    for( sModule = mExternalModule; *sModule != NULL; sModule++ )
    {
        IDE_TEST( (*sModule)->finalize() != IDE_SUCCESS );
    }

    IDE_TEST(iduMemMgr::free(mtfModulesByName) != IDE_SUCCESS);
    mtfModulesByName = NULL;

    IDE_TEST( mFuncMemoryPool.destroy() != IDE_SUCCESS);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtf::moduleByName( const mtfModule** aModule,
                          idBool*           aExist,
                          const void*       aName,
                          UInt              aLength )
{
    mtfNameIndex        sIndex;
    mtcName             sName;
    const mtfNameIndex* sFound;
    
    sName.length = aLength;
    sName.string = (void*)aName;
    sIndex.name  = &sName;
    
    sFound = (const mtfNameIndex*)idlOS::bsearch(                    &sIndex,
                                                            mtfModulesByName,
                                                    mtfNumberOfModulesByName,
                                                        ID_SIZEOF(mtfNameIndex),
                                          (PDL_COMPARE_FUNC)mtfCompareByName );

    if ( sFound == NULL )
    {
        *aExist = ID_FALSE;
    }
    else
    {
        *aExist = ID_TRUE;
        
        *aModule = sFound->module;
    }
    
    return IDE_SUCCESS;
}


IDE_RC
mtf::changeConvertCost()
{
/***********************************************************************
 *
 * Description :
 *
 *    ���� ��� �������� ���� ����� ��������
 *    Numeric ������ ���� �Լ��� �ƴ� Double�� ���� C Type ������
 *    ���� �Լ��� ���õ� Ȯ���� ���δ�.
 *
 * Implementation :
 *
 *    �̹� ������ ��ȯ Cost�߿��� ������(Smallint, Integer, Bigint)����
 *    Numeric�������� ��ȯ ����� ���� �����Ͽ�, ��ȯ �Լ� ���ÿ� �־�
 *    ������ ����ų �� �ִ� �Լ��� ���õǵ��� �Ѵ�.
 *
 *        - A3���� 2/3 ��� ���� ������ ��ȯ Cost��꿡 ����
 *          Numeric'2' / Numeric'2'�� ���� ������ ������ ���õȴ�.
 *        - A4������ Double'2' / Double'3'�� ���� ������ ������
 *          ���õȴ�.
 *
 *    �̿� ���� Convert Cost�� ������ ������ ��� ��� �����ڿ� ���Ͽ�
 *    ������ ���� �켱 ������ ���� �� ���ɼ��� ��������.
 *        - ������  op ������  : C Type
 *        - ������  op Numeric : C Type
 *        - Numeric op Numeric : Numeric Type
 *    �̷��� ó���� ������ ���� ���¿��� ��Ȯ�� ������ �߻��� �� �ִµ�,
 *    ���������� �Ǽ������� ǥ�������μ� ��ó�� �� �ִ�.
 *        - Numeric / 3   : Double ==> Loss �߻��� �� ����
 *        - Numeric / 3.0 : Numeric ==> Loss�� �߻����� ����
 *    �̷��� �ذ�å�� C Type�� ����� Numeric Type�� ������ ������ 10��
 *    �̻��� ���� �����Ͽ� �����Ͽ���.
 *
 *    ��� �����ڿ� ���� �Լ� ������ ������, ������ Cost���� ���� �־�
 *    �� ������ ����� �ùٸ� ó�� �Լ��� ������ �� �ְ� �Ѵ�.
 *
 ***********************************************************************/
    mtvTable * sTable;

    //-----------------------------------------------
    // SMALLINT -> ������ �� Cost ����
    //-----------------------------------------------
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdFloat.no,
                              mtdSmallint.no )
              != IDE_SUCCESS );
    
    mtf::saveCost[0][0] = sTable->cost;
    sTable->cost = MTV_COST_NATIVE2NUMERIC_PENALTY;
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumber.no,
                              mtdSmallint.no )
              != IDE_SUCCESS );
    
    mtf::saveCost[0][1] = sTable->cost;
    sTable->cost = MTV_COST_NATIVE2NUMERIC_PENALTY;

    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumeric.no,
                              mtdSmallint.no )
              != IDE_SUCCESS );
    
    mtf::saveCost[0][2] = sTable->cost;
    sTable->cost = MTV_COST_NATIVE2NUMERIC_PENALTY;

    //-----------------------------------------------
    // INTEGER -> ������ �� Cost ����
    //-----------------------------------------------
    
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdFloat.no,
                              mtdInteger.no )
              != IDE_SUCCESS );
    
    mtf::saveCost[1][0] = sTable->cost;
    sTable->cost = MTV_COST_NATIVE2NUMERIC_PENALTY;
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumber.no,
                              mtdInteger.no )
              != IDE_SUCCESS );
    
    mtf::saveCost[1][1] = sTable->cost;
    sTable->cost = MTV_COST_NATIVE2NUMERIC_PENALTY;
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumeric.no,
                              mtdInteger.no )
              != IDE_SUCCESS );
    
    mtf::saveCost[1][2] = sTable->cost;
    sTable->cost = MTV_COST_NATIVE2NUMERIC_PENALTY;

    //-----------------------------------------------
    // BIGINT -> ������ �� Cost ����
    //-----------------------------------------------
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdFloat.no,
                              mtdBigint.no )
              != IDE_SUCCESS );
    
    mtf::saveCost[2][0] = sTable->cost;
    sTable->cost = MTV_COST_NATIVE2NUMERIC_PENALTY;
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumber.no,
                              mtdBigint.no )
              != IDE_SUCCESS );
    
    mtf::saveCost[2][1] = sTable->cost;
    sTable->cost = MTV_COST_NATIVE2NUMERIC_PENALTY;
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumeric.no,
                              mtdBigint.no )
              != IDE_SUCCESS );
    
    mtf::saveCost[2][2] = sTable->cost;
    sTable->cost = MTV_COST_NATIVE2NUMERIC_PENALTY;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC
mtf::restoreConvertCost()
{
/***********************************************************************
 *
 * Description :
 *
 *    ::changeCovertCost()���� ������ ���� ������Ų��.
 *
 * Implementation :
 *
 ***********************************************************************/
    
    mtvTable * sTable;

    //-----------------------------------------------
    // SMALLINT -> ������ �� Cost ����
    //-----------------------------------------------
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdFloat.no,
                              mtdSmallint.no ) != IDE_SUCCESS );
    sTable->cost = mtf::saveCost[0][0];
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumber.no,
                              mtdSmallint.no ) != IDE_SUCCESS );
    sTable->cost = mtf::saveCost[0][1];

    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumeric.no,
                              mtdSmallint.no ) != IDE_SUCCESS );
    sTable->cost = mtf::saveCost[0][2];

    //-----------------------------------------------
    // INTEGER -> ������ �� Cost ����
    //-----------------------------------------------
    
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdFloat.no,
                              mtdInteger.no ) != IDE_SUCCESS );
    sTable->cost = mtf::saveCost[1][0];
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumber.no,
                              mtdInteger.no ) != IDE_SUCCESS );
    sTable->cost = mtf::saveCost[1][1];
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumeric.no,
                              mtdInteger.no ) != IDE_SUCCESS );
    sTable->cost = mtf::saveCost[1][2];

    //-----------------------------------------------
    // BIGINT -> ������ �� Cost ����
    //-----------------------------------------------
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdFloat.no,
                              mtdBigint.no ) != IDE_SUCCESS );
    sTable->cost = mtf::saveCost[2][0];
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumber.no,
                              mtdBigint.no ) != IDE_SUCCESS );
    sTable->cost = mtf::saveCost[2][1];
    
    IDE_TEST( mtv::tableByNo( (const mtvTable **) & sTable,
                              mtdNumeric.no,
                              mtdBigint.no ) != IDE_SUCCESS );
    sTable->cost = mtf::saveCost[2][2];
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtf::checkNeedFilter( mtcTemplate * aTmplate,
                             mtcNode     * aNode,
                             idBool      * aNeedFilter )
{
/***********************************************************************
 *
 * Description : Filter�� �ʿ����� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/    
    if ( idlOS::strncmp( (SChar*)aNode->module->names->string,
                         (SChar*)"LIKE",
                         4 ) == 0 )
    {
        // Like �� ���, Filter �ʿ� ���� �˻�
        IDE_TEST( checkNeedFilter4Like( aTmplate,
                                        aNode,
                                        aNeedFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        // ������ ���� �Լ� ��� ��, Filter �ʿ���
        // GeoContains, GeoCrosses, GeoDisjoint, GeoEquals, GeoIntersects,
        // GeoOverlaps, GeoTouches, GeoWithin, NotLike
        *aNeedFilter = ID_TRUE;
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtf::checkNeedFilter4Like( mtcTemplate * aTmplate,
                                  mtcNode     * aNode,
                                  idBool      * aNeedFilter )
{
/***********************************************************************
 *
 * Description : Filter�� �ʿ����� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/    
    const mtlModule * sLanguage;
    mtcNode         * sIndexNode;
    mtcNode         * sNode;
    mtcColumn       * sIndexColumn;
    mtcColumn       * sColumn;
    void            * sRowPtr;
    mtdEcharType    * sLikeEcharString;
    mtdCharType     * sLikeCharString;
    mtdCharType     * sEscapeChar = NULL;
    UChar           * sEscape;
    UChar           * sIndex;
    UChar           * sIndexPrev  = NULL;
    UChar           * sFence;
    idBool            sIsEqual;
    idBool            sIsEqual1;
    idBool            sIsEqual2;
    idBool            sIsEqual3;
    idBool            sNullEscape;
    UChar             sSize;
    
    mtcEncryptInfo    sDecryptInfo;
    UShort            sFormatPlainLength;
    UChar           * sFormatPlain;
    UChar             sDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];

    sIndexNode = aNode->arguments;
    sNode = sIndexNode->next;
    
    if ( ( ( aNode->lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_ABSENT ) &&
         ( ( aTmplate->rows[sNode->table].lflag & MTC_TUPLE_TYPE_MASK )
           == MTC_TUPLE_TYPE_CONSTANT ) )
    {
        //----------------------
        // Host Variable�� ���� ��� ������ ���
        //----------------------

        //-------------------------------------------
        // Like ���� string ����
        //-------------------------------------------
        
        sIndexColumn = & aTmplate->rows[sIndexNode->table].columns[sIndexNode->column];
        sColumn = & aTmplate->rows[sNode->table].columns[sNode->column];
        sLanguage = sColumn->language;
        
        if ( ( sColumn->module->id == MTD_ECHAR_ID ) ||
             ( sColumn->module->id == MTD_EVARCHAR_ID ) )
        {
            sRowPtr = aTmplate->rows[sNode->table].row;        
            sLikeEcharString = (mtdEcharType*)
                mtc::value( sColumn, sRowPtr, MTD_OFFSET_USE );
            
            // default policy�� �ƴѰ�� decrypt�� �����Ѵ�.
            if ( sColumn->policy[0] != '\0' )
            {
                sFormatPlain = sDecryptedBuf;
            
                if( sLikeEcharString->mCipherLength > 0 )
                {
                    IDE_TEST( aTmplate->getDecryptInfo( aTmplate,
                                                        sNode->baseTable,
                                                        sNode->baseColumn,
                                                        & sDecryptInfo )
                              != IDE_SUCCESS );
                
                    IDE_TEST( aTmplate->decrypt( & sDecryptInfo,
                                                 sColumn->policy,
                                                 sLikeEcharString->mValue,
                                                 sLikeEcharString->mCipherLength,
                                                 sFormatPlain,
                                                 & sFormatPlainLength )
                              != IDE_SUCCESS );
                
                    IDE_ASSERT_MSG( sFormatPlainLength <= sColumn->precision,
                                    "sFormatPlainLength : %"ID_UINT32_FMT"\n"
                                    "sColumn->precision : %"ID_UINT32_FMT"\n",
                                    sFormatPlainLength, sColumn->precision );

                }
                else
                {
                    sFormatPlainLength = 0;
                }
            }
            else
            {
                sFormatPlain = sLikeEcharString->mValue;
                sFormatPlainLength = sLikeEcharString->mCipherLength;
            }
        }
        else
        {
            sRowPtr = aTmplate->rows[sNode->table].row;        
            sLikeCharString = (mtdCharType*)
                mtc::value( sColumn, sRowPtr, MTD_OFFSET_USE );

            sFormatPlain = sLikeCharString->value;
            sFormatPlainLength = sLikeCharString->length;            
        }
        
        sIndex = sFormatPlain;
        sFence = sIndex + sFormatPlainLength;
            
        //-------------------------------------------
        // Escape ���� string ����
        //-------------------------------------------
        
        sNode = aNode->arguments->next->next;
        
        if ( sNode != NULL )
        {
            // escape ���ڸ� ������ ���
            sColumn = &aTmplate->rows[sNode->table].columns[sNode->column];
            sRowPtr = aTmplate->rows[sNode->table].row;
            sEscapeChar = (mtdCharType*)mtc::value( sColumn, sRowPtr,
                                                    MTD_OFFSET_USE );
            sEscape = sEscapeChar->value;

            sNullEscape = ID_FALSE;
        }
        else
        {
            // escape ���ڸ� �������� ���� ���
            sEscape = '\0';

            sNullEscape = ID_TRUE;
        }

        //-------------------------------------------
        // Key Filter�� �ʿ���� Key Range ���� �˻�
        //-------------------------------------------
        
        while ( sIndex < sFence )
        {
            sSize = mtl::getOneCharSize( sIndex,
                                         sFence,
                                         sLanguage );

            // fix BUG-19639
            if( sNullEscape == ID_FALSE )
            {
                
                sIsEqual = mtc::compareOneChar( sIndex,
                                                sSize,
                                                sEscape,
                                                sEscapeChar->length );
            }
            else
            {
                sIsEqual = ID_FALSE;
            }

            if( sIsEqual == ID_TRUE )
            {
                // To Fix PR-13004
                // ABR ������ ���Ͽ� ������Ų �� �˻��Ͽ��� ��

                (void)mtf::nextChar( sFence,
                                     &sIndex,
                                     &sIndexPrev,
                                     sLanguage );
                
                // escape ������ ���,
                // escape ���� ���ڰ� '%','_' �������� �˻�

                sSize =  mtl::getOneCharSize( sIndex,
                                              sFence,
                                              sLanguage );

                sIsEqual1 = mtc::compareOneChar( sIndex,
                                                 sSize,
                                                 sLanguage->specialCharSet[MTL_PC_IDX],
                                                 sLanguage->specialCharSize );

                sIsEqual2 = mtc::compareOneChar( sIndex,
                                                 sSize,
                                                 sLanguage->specialCharSet[MTL_UB_IDX],
                                                 sLanguage->specialCharSize );

                sIsEqual3 = mtc::compareOneChar( sIndex,
                                                 sSize,
                                                 sEscape,
                                                 sEscapeChar->length );

                // To Fix BUG-12578
                IDE_TEST_RAISE( (sIsEqual1 != ID_TRUE) &&
                                (sIsEqual2 != ID_TRUE) &&
                                (sIsEqual3 != ID_TRUE),
                                ERR_INVALID_LITERAL );
            }
            else
            {
                sIsEqual1 = mtc::compareOneChar( sIndex,
                                                 sSize,
                                                 sLanguage->specialCharSet[MTL_PC_IDX],
                                                 sLanguage->specialCharSize );

                sIsEqual2 = mtc::compareOneChar( sIndex,
                                                 sSize,
                                                 sLanguage->specialCharSet[MTL_UB_IDX],
                                                 sLanguage->specialCharSize );

                if( (sIsEqual1 == ID_TRUE) ||
                    (sIsEqual2 == ID_TRUE) )
                {
                    // Ư�������� ���
                    (void)mtf::nextChar( sFence,
                                         &sIndex,
                                         &sIndexPrev,
                                         sLanguage );
                    
                    break;
                }
                else
                {
                    // �Ϲ� ������ ���
                }
            }

            (void)mtf::nextChar( sFence,
                                 &sIndex,
                                 &sIndexPrev,
                                 sLanguage );
            
            if( ( sIndex - sFormatPlain ) > (UChar)MTC_LIKE_KEY_PRECISION )
            {
                // sIndex > 39
                break;
            }
            else
            {
                // Nothing To Do
            }
        }

        //-----------------
        // sIndex <= 39
        //-----------------

        if ( (sIndex == sFence) && (sIndexPrev != NULL) )
        {
            sSize =  mtl::getOneCharSize( sIndexPrev,
                                          sFence,
                                          sLanguage );

            if( ( sIndexPrev - sFormatPlain ) == (UChar)MTC_LIKE_KEY_PRECISION )
            {
                //----------------
                // sPrevIndex = 38
                //----------------
                
                sIsEqual = mtc::compareOneChar( sIndexPrev,
                                                sSize,
                                                sLanguage->specialCharSet[MTL_PC_IDX],
                                                sLanguage->specialCharSize );

                if( sIsEqual == ID_TRUE )
                {
                    // 'aaa%'�� ���� Ư�� ���� �ڿ� �Ϲ� ���ڰ� ���� ���,
                    // Key Range ������ Like ó�� ������

                    //----------------------------
                    // ��) i1 like '1234567890123456789012345678%'
                    //----------------------------

                    *aNeedFilter = ID_FALSE;
                }
                else
                {
                    //----------------------------
                    // ��) i1 like '12345678901234567890123456789'
                    //     i1 like '1234567890123456789012345678_'
                    //----------------------------

                    *aNeedFilter = ID_TRUE;
                }
            }
            else
            {
                sIsEqual = mtc::compareOneChar( sIndexPrev,
                                                sSize,
                                                sLanguage->specialCharSet[MTL_UB_IDX],
                                                sLanguage->specialCharSize );

                if ( sIsEqual == ID_TRUE )
                {
                    // 'aaa_'�� filter ó���� �ʿ���
                    *aNeedFilter = ID_TRUE;
                }
                else
                {
                    if( sFormatPlainLength > MTC_LIKE_KEY_PRECISION )
                    {
                        //------------------------------------------
                        // ��) UTF8 ���� �Ʒ��� like string = 39
                        //
                        //     I1 LIKE '������ �����ϱ� ���� ������'
                        //             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                        //     sIndex = 39
                        //     sPrevIndex = 36
                        //------------------------------------------

                        *aNeedFilter = ID_TRUE;
                    }
                    else
                    {
                        // 'aaa%'�� ���� Ư�� ���� �ڿ� �Ϲ� ���ڰ� ���� ���,
                        // 'aaa'�� ���� Ư�� ���ڰ� ���� �Ϲ� ���ڸ� �ִ� ���
                        // Key Range ������ Like ó�� ������

                        // BUG-36014
                        // �׷��� PROJ-1753���� like���� padding���ڸ� �����ϰ� �Ǿ�
                        // pattern�� %�� ������ ��찡 �ƴ϶�� filter�� �ʿ��ϴ�.
                        
                        sIsEqual = mtc::compareOneChar( sIndexPrev,
                                                        sSize,
                                                        sLanguage->specialCharSet[MTL_PC_IDX],
                                                        sLanguage->specialCharSize );
                
                        if ( ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                             &&
                             ( ( sIndexColumn->module->id == MTD_ECHAR_ID ) ||
                               ( sIndexColumn->module->id == MTD_NCHAR_ID ) ||
                               ( sIndexColumn->module->id == MTD_CHAR_ID ) ||
                               ( sIndexColumn->module->id == MTD_BIT_ID ) )
                             &&
                             ( ( sIsEqual == ID_FALSE ) ) )
                        {
                            // i1 like 'a'�� filter ó���� �ʿ���
                            *aNeedFilter = ID_TRUE;
                        }
                        else
                        {
                            *aNeedFilter = ID_FALSE;
                        }
                    }
                }
            }
        }
        else
        {
            // Ư�� ���� �ڿ� string�� ���� ���
            
            *aNeedFilter = ID_TRUE;
        }
    }
    else
    {
        //----------------------
        // Host Variable�� �ְų� ��� ������ �ƴ� ���, ������ filter ����
        //----------------------

        *aNeedFilter = ID_TRUE;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    {
        IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE ));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1075
// comparison table�� primitive type�� ���ؼ���
// ����Ǿ� �ֱ� ������
// UDT �� ���� üũ�ؼ� ������ ���־�� �Ѵ�.
IDE_RC mtf::getComparisonModule( const mtdModule** aModule,
                                  UInt aNo1,
                                  UInt aNo2 )
{

    IDE_TEST_RAISE( ( aNo1 >= mtd::getNumberOfModules() ) ||
                    ( aNo2 >= mtd::getNumberOfModules() ),
                    ERR_CONVERSION_NOT_APPLICABLE );
    
    *aModule = comparisonTable[aNo1][aNo2];

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1075
// comparison table�� primitive type�� ���ؼ���
// ����Ǿ� �ֱ� ������
// UDT �� ���� üũ�ؼ� ������ ���־�� �Ѵ�.
IDE_RC mtf::getSubModule1Arg( const mtfSubModule** aModule,
                              mtfSubModule**       aTable,
                              UInt aNo )
{

    IDE_TEST_RAISE( aNo >= mtd::getNumberOfModules(),
                    ERR_CONVERSION_NOT_APPLICABLE );

    *aModule = aTable[aNo];

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1075
// comparison table�� primitive type�� ���ؼ���
// ����Ǿ� �ֱ� ������
// UDT �� ���� üũ�ؼ� ������ ���־�� �Ѵ�.
IDE_RC mtf::getSubModule2Args( const mtfSubModule** aModule,
                               mtfSubModule***      aTable,
                               UInt                 aNo1,
                               UInt                 aNo2 )
{

    IDE_TEST_RAISE( ( aNo1 >= mtd::getNumberOfModules() ) ||
                    ( aNo2 >= mtd::getNumberOfModules() ),
                    ERR_CONVERSION_NOT_APPLICABLE );

    *aModule = aTable[aNo1][aNo2];

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool mtf::isEquiValidType( const mtdModule * aModule )
{
    idBool sResult;
    UInt   sDataGroup;
    
    sResult = ID_TRUE;

    sDataGroup = ( aModule->flag & MTD_GROUP_MASK );
    switch ( sDataGroup )
    {
        case MTD_GROUP_TEXT:
        case MTD_GROUP_NUMBER:
        case MTD_GROUP_DATE:
        case MTD_GROUP_INTERVAL:
            // ���� ����
            break;
        case MTD_GROUP_MISC:
            if ( (aModule == &mtdBoolean) ||
                 (aModule == &mtdByte) ||
                 (aModule == &mtdVarbyte) ||
                 (aModule == &mtdNibble) ||
                 (aModule == &mtdBit) ||
                 (aModule == &mtdVarbit) ||
                 (aModule == &mtdNull) )
            {
                // ���� ����
            }
            else
            {
                sResult = ID_FALSE;
            }
            
            break;
        default:
            ideLog::log( IDE_ERR_0,
                         "sDataGroup : %u\n",
                         sDataGroup );
                         
            IDE_ASSERT( 0 );
    }

    return sResult;
}


idBool mtf::isGreaterLessValidType( const mtdModule * aModule )
{
    idBool sResult;
    UInt   sDataGroup;
    
    sResult = ID_TRUE;

    sDataGroup = ( aModule->flag & MTD_GROUP_MASK );
    switch ( sDataGroup )
    {
        case MTD_GROUP_TEXT:
        case MTD_GROUP_NUMBER:
        case MTD_GROUP_DATE:
        case MTD_GROUP_INTERVAL:
            // ���� ����
            break;
        case MTD_GROUP_MISC:
            if ( (aModule == &mtdByte) ||
                 (aModule == &mtdVarbyte) ||
                 (aModule == &mtdNibble) ||
                 (aModule == &mtdBit) ||
                 (aModule == &mtdVarbit) ||
                 (aModule == &mtdNull) )
            {
                // EquiValid �� �޸� BLOB �� ��Һ񱳰� �ȵ�.
                // ���� ����
            }
            else
            {
                sResult = ID_FALSE;
            }
            
            break;
        default:
            ideLog::log( IDE_ERR_0,
                         "sDataGroup : %u\n",
                         sDataGroup );

            IDE_ASSERT( 0 );
    }

    return sResult;
}

mtlNCRet mtf::nextChar( const UChar      * aFence,
                        UChar           ** aIndex,
                        UChar           ** aIndexPrev,
                        const mtlModule  * aLanguage )
{
    if ( aIndexPrev != NULL )
    {
        *aIndexPrev = *aIndex;
    }
    else
    {
        // Nothing to do.
    }

    return aLanguage->nextCharPtr( aIndex, (UChar*)aFence );
}

IDE_RC mtf::copyString( UChar            * aDstStr,
                        UInt               aDstStrSize,
                        UInt             * aDstStrCopySize,
                        UChar            * aSrcStr,
                        UInt               aSrcStrSize,
                        const mtlModule  * aLanguage )
{
    UChar  * sString;
    UChar  * sStringFence;
    UChar  * sIndex;
    UChar  * sIndexPrev = NULL;
    UInt     sSize;
    
    if ( aDstStrSize >= aSrcStrSize )
    {
        if ( aSrcStrSize > 0 )
        {
            idlOS::memcpy( aDstStr, aSrcStr, aSrcStrSize );
        }
        else
        {
            // Nothing to do.
        }
        
        *aDstStrCopySize = aSrcStrSize;
    }
    else
    {
        if ( aLanguage == & mtlAscii )
        {
            sSize = aDstStrSize;
        }
        else
        {
            sString      = aSrcStr;
            sStringFence = sString + aSrcStrSize;
            sIndex       = sString;
        
            while ( sIndex < sStringFence )
            {
                (void)nextChar( sStringFence,
                                & sIndex,
                                & sIndexPrev,
                                aLanguage );

                if ( (UInt)(sIndex - sString) > aDstStrSize )
                {
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
            
            sSize = (UInt)( sIndexPrev - sString );
        }

        if ( sSize > 0 )
        {
            idlOS::memcpy( aDstStr, aSrcStr, sSize );
        }
        else
        {
            // Nothing to do.
        }
        
        *aDstStrCopySize = sSize;
    }
    
    return IDE_SUCCESS;
}

IDE_RC mtf::truncIncompletedString( UChar            * aString,
                                    UInt               aSize,
                                    UInt             * aTruncatedSize,
                                    const mtlModule  * aLanguage )
{
/***********************************************************************
 *
 * Description : BUG-38842
 *     ���ڿ��� �������� �������ڸ� truncate�Ѵ�.
 *
 * Implementation :
 *     6 byte������ ��� ó������ ���ڸ� Ȯ���ϰ�
 *     6 byte���� ū ��� ������ 6 byte�� �˻��Ѵ�.
 *
 ***********************************************************************/

    UChar   * sIndex;
    UChar   * sIndexTemp;
    UChar   * sFence;

    if ( aLanguage == & mtlAscii )
    {
        *aTruncatedSize = aSize;
    }
    else
    {
        if ( aSize <= MTL_MAX_PRECISION + MTL_MAX_PRECISION )
        {
            // ó������ ���ڸ� Ȯ���Ѵ�.
            sIndex = aString;
            sFence = aString + aSize;
        }
        else
        {
            // ������ 6byte�� Ȯ���Ѵ�.
            sIndex = aString + aSize - MTL_MAX_PRECISION - MTL_MAX_PRECISION;
            sFence = aString + aSize;
        }

        while ( sIndex < sFence )
        {
            sIndexTemp = sIndex;

            if ( aLanguage->nextCharPtr( & sIndexTemp, sFence )
                 == NC_MB_INCOMPLETED )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }

            sIndex = sIndexTemp;
        }

        *aTruncatedSize = (UInt)(sIndex - aString);
    }

    return IDE_SUCCESS;
}

IDE_RC mtf::makeUFromChar(
    const mtlModule  * aSourceCharSet,
    UChar            * aSourceIndex,
    UChar            * aSourceFence,
    UChar            * aResultValue,
    UChar            * aResultFence,
    UInt             * aCharLength )
/***********************************************************************
 *
 * Description : PROJ-1579 NCHAR
 *      NCHAR �Ǵ� CHAR Ÿ�� ���ڿ��� U Ÿ������ �����Ѵ�.
 *
 *      ex) AB��CD => AB\C548CD
 *
 *      ���� ��� ���ڿ��� ������ ASCII�̴�.
 *
 * Implementation :
 *
 ***********************************************************************/
{
    const mtlModule   * sU16CharSet;
    UChar             * sResultValue;
    UChar             * sTempIndex;
    idnCharSetList      sIdnSourceCharSet;
    idnCharSetList      sIdnU16CharSet;
    UChar               sU16Result[MTL_UTF16_PRECISION];
    SInt                sSrcRemain = 0;
    SInt                sDestRemain = 0;
    idBool              sIsAscii = ID_FALSE;

    // '0' ~ '9', 'A' ~ 'F'
    const UChar sHexValue[16] = {
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70 };

    sResultValue = aResultValue;

    sU16CharSet = & mtlUTF16;

    sIdnSourceCharSet = mtl::getIdnCharSet( aSourceCharSet );
    sIdnU16CharSet    = mtl::getIdnCharSet( sU16CharSet );

    // ĳ���� �� ��ȯ �� ����ϴ� ������ ����
    sDestRemain = aResultFence - aResultValue;

    // �ҽ��� ����
    sSrcRemain = aSourceFence - aSourceIndex;

    while( aSourceIndex < aSourceFence )
    {
        IDE_TEST_RAISE( aResultValue >= aResultFence,
                        ERR_INVALID_DATA_LENGTH );

        if( aSourceCharSet->id == MTL_UTF16_ID )
        {
            if( IDN_IS_UTF16_ASCII_PTR( aSourceIndex ) == ID_TRUE )
            {
                *aResultValue ++ = *aSourceIndex;
                sDestRemain--;
                sIsAscii = ID_TRUE;
            }
            else
            {
                // Nothing to do
            }
        }
        else
        {
            if( IDN_IS_ASCII(*aSourceIndex) == ID_TRUE )
            {
                *aResultValue ++ = *aSourceIndex;
                sDestRemain--;

                sIsAscii = ID_TRUE;
            }
            else
            {
                // Nothing to do
            }
        }

        if( sIsAscii == ID_TRUE )
        {
            sIsAscii = ID_FALSE;
        }
        else
        {
            // aResultValue�� ������ ASCII�̴�. ���� ++�� �̵��ص� ������.
            *aResultValue ++ = '\\';
            sDestRemain--;

            // result ������ �ִ��� �̸� üũ
            IDE_TEST_RAISE( aResultValue  >= aResultFence - 3,
                            ERR_INVALID_DATA_LENGTH );

            if( sIdnSourceCharSet != sIdnU16CharSet )
            {
                // sU16Result�� UTF16 ���� �޾ƿ´�.
                IDE_TEST( convertCharSet( sIdnSourceCharSet,
                                          sIdnU16CharSet,
                                          aSourceIndex,
                                          sSrcRemain,
                                          sU16Result,
                                          & sDestRemain,
                                          -1 /* mNlsNcharConvExcp */ )
                          != IDE_SUCCESS );

                // UTF16���� UTF16 �ڵ�����Ʈ�� ������Ų��.
                // ���� ��� 2 ����Ʈ C5 48�� ���
                // 4 ����Ʈ�� C 5 4 8�� �����Ѵ�.
                *aResultValue ++ = sHexValue[(sU16Result[0] >> 4 )];
                *aResultValue ++ = sHexValue[(sU16Result[0] & 0x0F )];
                *aResultValue ++ = sHexValue[(sU16Result[1] >> 4 )];
                *aResultValue ++ = sHexValue[(sU16Result[1] & 0x0F )];
            }
            else
            {
                *aResultValue ++ = sHexValue[(aSourceIndex[0] >> 4 )];
                *aResultValue ++ = sHexValue[(aSourceIndex[0] & 0x0F )];
                *aResultValue ++ = sHexValue[(aSourceIndex[1] >> 4 )];
                *aResultValue ++ = sHexValue[(aSourceIndex[1] & 0x0F )];
            }

            sDestRemain -= 4;
        }

        sTempIndex = aSourceIndex;

        if( aSourceCharSet->nextCharPtr( (UChar **)& aSourceIndex,
                                         aSourceFence )
            == NC_MB_INCOMPLETED )
        {
            break;
        }
        else
        {
            sSrcRemain -= ( aSourceIndex - sTempIndex );
        }
    }

    *aCharLength = aResultValue - sResultValue;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DATA_LENGTH );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_LENGTH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtf::getQuantFuncCalcType( mtcNode *              aNode,
							      mtcTemplate *          aTemplate,
								  mtfQuantFuncCalcType * aCalcType )
{
    const mtfModule * sMtfModule;
    const mtdModule * sMtdModule;
    idBool            sCouldFast;
    mtcNode *         sNode;

    sCouldFast = ID_TRUE;

    IDE_TEST_RAISE( aNode->arguments->next == NULL,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    sMtfModule = aNode->arguments->module;

    if ( ( aNode->arguments->next->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) < 2 )
    {
        /* does not need to fast calculate */
        sCouldFast = ID_FALSE;
    }
    /* Left Type allow "VALUE" or "COLUMN" */
    else if ( idlOS::strncmp( (SChar*) sMtfModule->names->string,
                             (const SChar*)"VALUE",
                             5 ) == 0 ||
              idlOS::strncmp( (SChar*) sMtfModule->names->string,
                             (const SChar*)"COLUMN",
                             6 ) == 0 )
    {
        sNode      = aNode->arguments->next->arguments;
        sMtdModule = aTemplate->rows[sNode->table].columns[sNode->column].module;

        for ( sNode = sNode->next ;
              sNode != NULL ;
              sNode = sNode->next )
        {
            sMtfModule = sNode->module;
            /* Not VALUE or
             * Not SAME TYPE (same type == same module) */
            if ( idlOS::strncmp( (SChar*) sMtfModule->names->string,
                                 (const SChar*)"VALUE",
                                 5 ) != 0 ||
                 sMtdModule != aTemplate->rows[sNode->table].columns[sNode->column].module)
            {
                sCouldFast = ID_FALSE;
                break;
            }
        }
    }
    else
    {
        sCouldFast = ID_FALSE;
    }

    if ( sCouldFast == ID_TRUE )
    {
        *aCalcType = MTF_QUANTIFY_FUNC_FAST_CALC;
    }
    else
    {
        *aCalcType = MTF_QUANTIFY_FUNC_NORMAL_CALC;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtf::allocQuantArgData( mtcNode     * aNode,
							   mtcTemplate * aTemplate,
							   mtcStack    * aStack,
							   mtcCallBack * aCallBack )
{
	extern mtdModule mtdBinary;

    UInt * sCalculateInfo;
    UInt   sDataSize;
    ULong  sArgCount;

    /*  Quantify Function's ArgData for Fast Calculation
     *
     *  sizeof(mtfQuantFuncArgData) +
     *  sizeof(mtcStack) * in_list# +
     *  sizeof(idBool)   * in_list#    */
    sArgCount = aNode->arguments->next->lflag & MTC_NODE_ARGUMENT_COUNT_MASK;
    sDataSize = idlOS::align( ID_SIZEOF(mtfQuantFuncArgData), 8 ) +
                idlOS::align( sArgCount * ID_SIZEOF(mtcStack), 8 ) +
                idlOS::align( sArgCount * ID_SIZEOF(idBool), 8 );

    IDE_TEST( mtc::initializeColumn( aStack[0].column + 1,
                                     &mtdBinary,
                                     1,
                                     sDataSize,
                                     0 )
              != IDE_SUCCESS );

    /* Setup at CalculateInfo */
    IDE_TEST( aCallBack->alloc( aCallBack->info,
                                ID_SIZEOF( UInt ),
                                (void**) &sCalculateInfo )
              != IDE_SUCCESS );
    *sCalculateInfo = aTemplate->execInfoCnt++;
    aTemplate->rows[aNode->table].execute[aNode->column + 1].calculateInfo = sCalculateInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtf::buildQuantArgData( mtcNode *     aNode,
		                       mtcStack *    aStack,
		                       SInt          aRemain,
		                       void *        aInfo,
                               mtcTemplate * aTemplate )
{
    const mtdModule *     sMtdModule = NULL;
    mtcStack *            sStackList;
    mtcNode *             sNode;
    UInt                  sCount;
    UInt                  sFence;

    mtfQuantFuncArgData * sData;

    /* variable for Sorting */
    UInt                  i;
    UInt                  j;
    mtdValueInfo          sValueInfo1;
    mtdValueInfo          sValueInfo2;
    mtcStack              sSwapStack;
    idBool                sSwapBool;

    IDE_TEST( postfixCalculate( aNode,
                                aStack,
                                aRemain,
                                aInfo,
                                aTemplate )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( aNode->arguments->next == NULL,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    sData               = (mtfQuantFuncArgData*) getArgDataPtr( aNode, aTemplate );
    sFence              = aStack[2].column->precision;
    sData->mSelf        = sData;
    sData->mElements    = sFence;
    sData->mStackList   = (mtcStack*)idlOS::align( &sData[1], 8 );
    sData->mIsNullList  = (idBool*)idlOS::align( &sData->mStackList[sData->mElements], 8 );

    sNode      = aNode->arguments->next->arguments;
    sStackList = (mtcStack*)aStack[2].value ;

    for( sCount     = 0 ;
         sCount  < sFence ;
         sCount++, sNode = sNode->next )
    {
        sData->mStackList[sCount] = sStackList[sCount];

        sMtdModule = sStackList[sCount].column->module;

        if ( sMtdModule->isNull( sStackList[sCount].column,
                                 sStackList[sCount].value ) == ID_TRUE )
        {
            sData->mIsNullList[sCount] = ID_TRUE;
        }
        else
        {
            sData->mIsNullList[sCount] = ID_FALSE;
        }
    }

    /* Sort StackList for use Biniry Search */
    sStackList = sData->mStackList;

    for ( i = 0 ; i < sFence - 1 ; i++ )
    {
        if ( sData->mIsNullList[i] != ID_TRUE )
        {
            for ( j = i + 1 ; j < sFence ; j++ )
            {
                if ( sData->mIsNullList[j] != ID_TRUE )
                {
                    sValueInfo1.column = sStackList[i].column;
                    sValueInfo1.value  = sStackList[i].value;
                    sValueInfo1.flag   = MTD_OFFSET_USELESS;

                    sValueInfo2.column = sStackList[j].column;
                    sValueInfo2.value  = sStackList[j].value;
                    sValueInfo2.flag   = MTD_OFFSET_USELESS;

                    if ( sMtdModule->logicalCompare[MTD_COMPARE_ASCENDING]( &sValueInfo1,
                                                                            &sValueInfo2 ) > 0 )
                    {
                        sSwapStack            = sStackList[i];
                        sStackList[i]         = sStackList[j];
                        sStackList[j]         = sSwapStack;

                        sSwapBool             = sData->mIsNullList[i];
                        sData->mIsNullList[i] = sData->mIsNullList[j];
                        sData->mIsNullList[j] = sSwapBool;
                    }
                }
                else
                {
                    sSwapStack            = sStackList[i];
                    sStackList[i]         = sStackList[j];
                    sStackList[j]         = sSwapStack;

                    sSwapBool             = sData->mIsNullList[i];
                    sData->mIsNullList[i] = sData->mIsNullList[j];
                    sData->mIsNullList[j] = sSwapBool;

                    // BUG-43784 NULL�� swap�ϸ� �񱳸� �����ؾ� �Ѵ�.
                    break;
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtf::hex2Ascii( UChar aIn, UChar *aOut )
{
    if ( ( aIn >= '0') && ( aIn <= '9') )
    {
        *aOut = aIn - '0';
    }
    else
    {
        if ( ( aIn >= 'A') && ( aIn <= 'Z') )
        {
            *aOut = aIn + 10 - 'A';
        }
        else
        {
            if ( ( aIn >= 'a') && ( aIn <= 'z') )
            {
                *aOut = aIn + 10 - 'a';
            }
            else
            {
                IDE_RAISE( ERR_INVALID_LITERAL );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-2527 WITHIN GROUP AGGR
IDE_RC mtf::initializeFuncDataBasicInfo( mtfFuncDataBasicInfo  * aFuncDataBasicInfo,
                                         iduMemory             * aMemoryMgr )
{
    aFuncDataBasicInfo->memoryMgr = aMemoryMgr;

    return IDE_SUCCESS;
}

IDE_RC mtf::allocFuncDataMemory( iduMemory ** aMemoryMgr )
{
    void  * sAllocPtr;
    
    IDU_FIT_POINT( "mtf::allocFuncDataMemory::alloc::sBuffer",
                         mtERR_ABORT_MEMORY_ALLOCATION );
    IDE_TEST( mFuncMemoryPool.alloc( (void**)&sAllocPtr )
              != IDE_SUCCESS );

    (*aMemoryMgr) = new (sAllocPtr) iduMemory();
    (void)(*aMemoryMgr)->init( IDU_MEM_MT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void mtf::freeFuncDataMemory( iduMemory * aMemoryMgr )
{
    aMemoryMgr->destroy();
    
    (void) mFuncMemoryPool.memfree( aMemoryMgr );
}

IDE_RC mtf::setKeepOrderData( mtcNode          * aNode,
                              mtcStack         * aStack,
                              iduMemory        * aMemory,
                              UChar            * aDenseRankOpt,
                              mtfKeepOrderData * aKeepOrderData,
                              UInt               aOrderByStackPos )
{
    mtcNode  * sNode; 
    mtcStack * sStack;
    UChar    * sValue;
    UInt     * sOpt;
    UInt       sSize;
    UInt       sRowSize = 0;
    UInt       i        = 0;

    for ( sNode = aNode;
          sNode != NULL;
          sNode = sNode->next, i++ )
    {
        sOpt = &aKeepOrderData->mOptions[i];
        if ( *aDenseRankOpt == 'F' )
        {
            if ( ( sNode->lflag & MTC_NODE_WITHIN_GROUP_ORDER_MASK )
                 == MTC_NODE_WITHIN_GROUP_ORDER_DESC )
            {
                if ( ( sNode->lflag & MTC_NODE_NULLS_OPT_EXIST_MASK )
                     == MTC_NODE_NULLS_OPT_EXIST_FALSE )
                {
                    *sOpt = MTF_KEEP_FIRST_ORDER_DESC;
                }
                else
                {
                    if ( ( sNode->lflag & MTC_NODE_WITHIN_GROUP_NULLS_MASK )
                         == MTC_NODE_WITHIN_GROUP_NULLS_FIRST )
                    {
                        *sOpt = MTF_KEEP_FIRST_ORDER_DESC_NULLS_FIRST;
                    }
                    else
                    {
                        *sOpt = MTF_KEEP_FIRST_ORDER_DESC_NULLS_LAST;
                    }
                }
            }
            else
            {
                if ( ( sNode->lflag & MTC_NODE_NULLS_OPT_EXIST_MASK )
                     == MTC_NODE_NULLS_OPT_EXIST_FALSE )
                {
                    *sOpt = MTF_KEEP_FIRST_ORDER_ASC;
                }
                else
                {
                    if ( ( sNode->lflag & MTC_NODE_WITHIN_GROUP_NULLS_MASK )
                         == MTC_NODE_WITHIN_GROUP_NULLS_FIRST )
                    {
                        *sOpt = MTF_KEEP_FIRST_ORDER_ASC_NULLS_FIRST;
                    }
                    else
                    {
                        *sOpt = MTF_KEEP_FIRST_ORDER_ASC_NULLS_LAST;
                    }
                }
            }
        }
        else
        {
            if ( ( sNode->lflag & MTC_NODE_WITHIN_GROUP_ORDER_MASK )
                 == MTC_NODE_WITHIN_GROUP_ORDER_DESC )
            {
                if ( ( sNode->lflag & MTC_NODE_NULLS_OPT_EXIST_MASK )
                     == MTC_NODE_NULLS_OPT_EXIST_FALSE )
                {
                    *sOpt = MTF_KEEP_LAST_ORDER_DESC;
                }
                else
                {
                    if ( ( sNode->lflag & MTC_NODE_WITHIN_GROUP_NULLS_MASK )
                         == MTC_NODE_WITHIN_GROUP_NULLS_FIRST )
                    {
                        *sOpt = MTF_KEEP_LAST_ORDER_DESC_NULLS_FIRST;
                    }
                    else
                    {
                        *sOpt = MTF_KEEP_LAST_ORDER_DESC_NULLS_LAST;
                    }
                }
            }
            else
            {
                if ( ( sNode->lflag & MTC_NODE_NULLS_OPT_EXIST_MASK )
                     == MTC_NODE_NULLS_OPT_EXIST_FALSE )
                {
                    *sOpt = MTF_KEEP_LAST_ORDER_ASC;
                }
                else
                {
                    if ( ( sNode->lflag & MTC_NODE_WITHIN_GROUP_NULLS_MASK )
                         == MTC_NODE_WITHIN_GROUP_NULLS_FIRST )
                    {
                        *sOpt = MTF_KEEP_LAST_ORDER_ASC_NULLS_FIRST;
                    }
                    else
                    {
                        *sOpt = MTF_KEEP_LAST_ORDER_ASC_NULLS_LAST;
                    }
                }
            }
        }
    }
    aKeepOrderData->mCount = i;

    for ( i = 0; i < aKeepOrderData->mCount; i++ )
    {
        sSize = idlOS::align( sRowSize, aStack[ i + aOrderByStackPos].column->module->align );
        aKeepOrderData->mOffsets[i] = sSize + sRowSize;
        sRowSize += ( sSize + aStack[i + aOrderByStackPos].column->column.size );
    }

    sRowSize = idlOS::align8( sRowSize );

    IDE_TEST_RAISE( sRowSize <= 0, ERR_UNEXPECTED );

    IDU_FIT_POINT_RAISE( "setKeepOrderData::alloc::mData",
                         ERR_MEMORY_ALLOCATION );
    IDE_TEST_RAISE( aMemory->alloc( sRowSize, (void**)&(aKeepOrderData->mData) )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOCATION );

    for ( i = 0; i < aKeepOrderData->mCount; i++ )
    {
        sValue = aKeepOrderData->mData + aKeepOrderData->mOffsets[i];
        sStack = &aStack[i + aOrderByStackPos];
        idlOS::memcpy( sValue,
                       sStack->value,
                       sStack->column->module->actualSize( sStack->column,
                                                           sStack->value ) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOCATION )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION( ERR_UNEXPECTED )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR, "mtf::setKeepOrderData",
                                  "Row Size <= 0" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void mtf::getKeepAction( mtcStack         * aStack,
                         mtfKeepOrderData * aKeepOrderData,
                         UInt             * aAction )
{
    UChar           * sValue;
    const mtdModule * sModule;
    mtdValueInfo      sValueInfo1;
    mtdValueInfo      sValueInfo2;
    SInt              sRet;
    idBool            sAllEqual = ID_TRUE;
    mtcStack        * sStack;
    UInt              sAction = 0;
    UInt              i;
    idBool            sIsNull1;
    idBool            sIsNull2;

    for ( i = 0; i < aKeepOrderData->mCount; i++ )
    {
        sValue  = aKeepOrderData->mData + aKeepOrderData->mOffsets[i];
        sModule = aStack[i].column->module;

        sValueInfo1.column = aStack[i].column;
        sValueInfo1.value  = sValue;
        sValueInfo1.flag   = MTD_OFFSET_USELESS;

        sValueInfo2.column = aStack[i].column;
        sValueInfo2.value  = aStack[i].value;
        sValueInfo2.flag   = MTD_OFFSET_USELESS;

        sRet = sModule->logicalCompare[MTD_COMPARE_ASCENDING]( &sValueInfo1,
                                                               &sValueInfo2 );
        if ( sRet != 0 )
        {
            sAllEqual = ID_FALSE;

            switch ( aKeepOrderData->mOptions[i] )
            {
                case MTF_KEEP_FIRST_ORDER_ASC:
                case MTF_KEEP_FIRST_ORDER_ASC_NULLS_LAST:
                case MTF_KEEP_LAST_ORDER_DESC:
                case MTF_KEEP_LAST_ORDER_DESC_NULLS_FIRST:
                    if ( sRet > 0 )
                    {
                        sAction = MTF_KEEP_ACTION_INIT;
                    }
                    else
                    {
                        sAction = MTF_KEEP_ACTION_SKIP;
                    }
                    break;
                case MTF_KEEP_FIRST_ORDER_ASC_NULLS_FIRST:
                case MTF_KEEP_LAST_ORDER_DESC_NULLS_LAST:
                    sIsNull1 = sModule->isNull( aStack[i].column, sValue );
                    sIsNull2 = sModule->isNull( aStack[i].column, aStack[i].value );
                    if ( ( sIsNull1 == ID_FALSE ) && ( sIsNull2 == ID_FALSE ) )
                    {
                        if ( sRet > 0 )
                        {
                            sAction = MTF_KEEP_ACTION_INIT;
                        }
                        else
                        {
                            sAction = MTF_KEEP_ACTION_SKIP;
                        }
                    }
                    else
                    {
                        if ( sIsNull1 == ID_TRUE )
                        {
                            sAction = MTF_KEEP_ACTION_SKIP;
                        }
                        else
                        {
                            /* Nothing to do */
                        }

                        if ( sIsNull2 == ID_TRUE )
                        {
                            sAction = MTF_KEEP_ACTION_INIT;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }
                    break;
                case MTF_KEEP_FIRST_ORDER_DESC:
                case MTF_KEEP_FIRST_ORDER_DESC_NULLS_FIRST:
                case MTF_KEEP_LAST_ORDER_ASC:
                case MTF_KEEP_LAST_ORDER_ASC_NULLS_LAST:
                    if ( sRet < 0 )
                    {
                        sAction = MTF_KEEP_ACTION_INIT;
                    }
                    else
                    {
                        sAction = MTF_KEEP_ACTION_SKIP;
                    }
                    break;
                case MTF_KEEP_FIRST_ORDER_DESC_NULLS_LAST:
                case MTF_KEEP_LAST_ORDER_ASC_NULLS_FIRST:
                    sIsNull2 = sModule->isNull( aStack[i].column, aStack[i].value );
                    if ( sIsNull2 == ID_FALSE )
                    {
                        if ( sRet < 0 )
                        {
                            sAction = MTF_KEEP_ACTION_INIT;
                        }
                        else
                        {
                            sAction = MTF_KEEP_ACTION_SKIP;
                        }
                    }
                    else
                    {
                        sAction = MTF_KEEP_ACTION_SKIP;
                    }
                    break;
                default:
                    break;
            }
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sAllEqual == ID_TRUE )
    {
        *aAction = MTF_KEEP_ACTION_AGGR;
    }
    else
    {
        if ( sAction == MTF_KEEP_ACTION_INIT )
        {
            for ( i = 0; i < aKeepOrderData->mCount; i++ )
            {
                sValue = aKeepOrderData->mData + aKeepOrderData->mOffsets[i];
                sStack = &aStack[i];
                idlOS::memcpy( sValue,
                               sStack->value,
                               sStack->column->module->actualSize( sStack->column,
                                                                   sStack->value ) );
            }
        }
        else
        {
            /* Nothing to do */
        }

        *aAction = sAction;
    }
}
