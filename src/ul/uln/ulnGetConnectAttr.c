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

#include <uln.h>
#include <ulnPrivate.h>
#include <ulnGetConnectAttr.h>

/*
 * ULN_SFID_77
 * SQLGetConnectAttr(), DBC, C2
 *
 *	    -- [1]
 *	    (08003) [2]
 *
 *	where
 *      [1] The Attribute argument was
 *              SQL_ATTR_ACCESS_MODE,
 *              SQL_ATTR_AUTOCOMMIT,
 *              SQL_ATTR_LOGIN_TIMEOUT,
 *              SQL_ATTR_ODBC_CURSORS,
 *              SQL_ATTR_TRACE,
 *              or SQL_ATTR_TRACEFILE,
 *              or a value had been set for the connection attribute.
 *
 *      [2] The Attribute argument was not
 *              SQL_ATTR_ACCESS_MODE,
 *              SQL_ATTR_AUTOCOMMIT,
 *              SQL_ATTR_LOGIN_TIMEOUT,
 *              SQL_ATTR_ODBC_CURSORS,
 *              SQL_ATTR_TRACE,
 *              or SQL_ATTR_TRACEFILE,
 *              and a value had not been set for the connection attribute.
 *
 *      BUGBUG : �ؼ��ϱ� ����� �����ϴ�-_-;
 *               �ϴ�, �Ӽ��� ������ ������ �Ӽ��� �ƴ� ������ ������ 08003 �� ������ ����.
 *               �ƹ����� �Ӽ��� ����ڰ� �����ߴ��� ���ߴ����� �����ϴ� �����ΰ���
 *               ������ �־�� �� �� ������,
 *               �ϴ�, ����̹� �Ŵ����� ������� �� �����ڵ��̹Ƿ� ���� �׳� ��ô ���� -_-;
 */
ACI_RC ulnSFID_77(ulnFnContext *aFnContext)
{
    acp_sint32_t aAttrCode;

    if(aFnContext->mWhere == ULN_STATE_ENTRY_POINT)
    {
        aAttrCode = *(acp_sint32_t *)(aFnContext->mArgs);

        switch(aAttrCode)
        {
            case SQL_ATTR_ACCESS_MODE:
            case SQL_ATTR_AUTOCOMMIT:
            case SQL_ATTR_LOGIN_TIMEOUT:
            case SQL_ATTR_ODBC_CURSORS:
            case SQL_ATTR_TRACE:
            case SQL_ATTR_TRACEFILE:
            case SQL_ATTR_CONNECTION_DEAD: /* BUG-15741 */

            case ALTIBASE_DATE_FORMAT:     /* BUG-15907 Before Gets */
            case ALTIBASE_APP_INFO:
            case ALTIBASE_NLS_USE:
            case ALTIBASE_NLS_NCHAR_LITERAL_REPLACE:  /* PROJ-1579 NCHAR */
            case ALTIBASE_NLS_CHARACTERSET:           /* PROJ-1579 NCHAR */
            case ALTIBASE_NLS_NCHAR_CHARACTERSET:     /* PROJ-1579 NCHAR */
            case ALTIBASE_EXPLAIN_PLAN:
            case ALTIBASE_STACK_SIZE:
            case ALTIBASE_PREFER_IPV6:
            /* BUG-31144 */
            case ALTIBASE_MAX_STATEMENTS_PER_SESSION:
            case ALTIBASE_SESSION_ID:      /* BUG-34447 isql SET NUMF[ORMAT] fmt */

            /* ?? - does it let get before ?? */
            case ALTIBASE_OPTIMIZER_MODE      :
            /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
            case ALTIBASE_DDL_TIMEOUT         :
            case ALTIBASE_UTRANS_TIMEOUT      :
            case ALTIBASE_FETCH_TIMEOUT       :
            case ALTIBASE_IDLE_TIMEOUT        :
            case ALTIBASE_HEADER_DISPLAY_MODE :
            /* PROJ-2047 Strengthening LOB - LOBCACHE */
            case ALTIBASE_LOB_CACHE_THRESHOLD :

            case SQL_ATTR_PORT:
            case SQL_ATTR_DISCONNECT_BEHAVIOR:
            case SQL_ATTR_CONNECTION_TIMEOUT:
            case SQL_ATTR_CURRENT_CATALOG:
            case SQL_ATTR_METADATA_ID:
            case SQL_ATTR_FAILOVER:
            case SQL_BATCH_SUPPORT:
            case SQL_ATTR_CONNECTION_POOLING:
            case SQL_ATTR_ASYNC_ENABLE:
            case SQL_ATTR_QUIET_MODE:
            case SQL_ATTR_QUERY_TIMEOUT:
            case SQL_ATTR_TXN_ISOLATION:
            case SQL_ATTR_PACKET_SIZE:
                break;

            default:
                ACI_RAISE(LABEL_ABORT_NO_CONNECTION);
                break;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_ABORT_NO_CONNECTION)
    {
        ulnError(aFnContext, ulERR_ABORT_NO_CONNECTION, "");
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static void getString(acp_char_t *aAttr,void *aValPtr, acp_sint32_t aValLen, acp_sint32_t   *aIndPtr)
{
    if(aAttr == NULL)
    {
        acpSnprintf((acp_char_t*)aValPtr, aValLen, "");
    }
    else
    {
        acpSnprintf((acp_char_t*)aValPtr, aValLen, "%s", aAttr);
    }

    if(aIndPtr != NULL)
    {
        *aIndPtr = acpCStrLen((acp_char_t*)aValPtr, aValLen);
    }
}

SQLRETURN ulnGetConnectAttr(ulnDbc       *aDbc,
                            acp_sint32_t  aAttrCode,
                            void         *aValPtr,
                            acp_sint32_t  aValLen,
                            acp_sint32_t *aIndPtr)
{
    ULN_FLAG(sNeedExit);
    ulnFnContext  sFnContext;
    ulnDbc       *sDbc;
    acp_bool_t    sIsClosed;

    /* proj-1573 XA */
    sDbc = ulnDbcSwitch(aDbc);

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_GETCONNECTATTR, sDbc, ULN_OBJ_TYPE_DBC);

    /*
     * Enter
     */
    ACI_TEST(ulnEnter(&sFnContext, (void *)(&aAttrCode)) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    ACI_TEST_RAISE( aValPtr == NULL, ERR_IH000);

    /*
     * BUGBUG : DBC ����ü�� �ٷ� access ���� ����, �Լ��� ����ϵ��� ����
     */
    switch(aAttrCode)
    {
        case ALTIBASE_DATE_FORMAT:
            (void)getString(sDbc->mDateFormat, aValPtr, aValLen, aIndPtr);
            break;

        case ALTIBASE_APP_INFO:
            if(sDbc->mAppInfo == NULL)
            {
                acpSnprintf((acp_char_t*)aValPtr, aValLen, "");
            }
            else
            {
                acpSnprintf((acp_char_t*)aValPtr, aValLen, "%s", sDbc->mAppInfo);
            }

            if(aIndPtr != NULL)
            {
                *((acp_uint32_t*)aIndPtr) = acpCStrLen((acp_char_t*)aValPtr, aValLen);
            }
            break;


        case ALTIBASE_NLS_USE:
            (void)getString( ulnDbcGetNlsLangString(sDbc), aValPtr, aValLen, aIndPtr);
            break;

        //PROJ-1645 UL Failover.
        case ALTIBASE_ALTERNATE_SERVERS:
            (void)getString( ulnDbcGetAlternateServer(sDbc), aValPtr, aValLen, aIndPtr);
            break;
        case ALTIBASE_CONNECTION_RETRY_COUNT:
            *(acp_uint32_t*)aValPtr = ulnDbcGetConnectionRetryCount(sDbc);
            break;
        case ALTIBASE_CONNECTION_RETRY_DELAY:
            *(acp_uint32_t*)aValPtr = ulnDbcGetConnectionRetryDelay(sDbc);
            break;
        case ALTIBASE_SESSION_FAILOVER:
            *(acp_uint32_t*)aValPtr = (acp_uint32_t)ulnDbcGetSessionFailover(sDbc);
            break;

        /* bug-31468: adding conn-attr for trace logging */
        case ALTIBASE_TRACELOG:
            *(acp_uint32_t*)aValPtr = ulnDbcGetTraceLog(sDbc);
            break;

        // PROJ-1579 NCHAR
        case ALTIBASE_NLS_NCHAR_LITERAL_REPLACE:
            *(acp_uint32_t*)aValPtr = sDbc->mNlsNcharLiteralReplace;
            break;

        // PROJ-1579 NCHAR
        case ALTIBASE_NLS_CHARACTERSET_VALIDATION:
            *(acp_uint32_t*)aValPtr = sDbc->mNlsCharactersetValidation;
            break;

        // PROJ-1579 NCHAR
        // �����ͺ��̽� ĳ���� ���� �ǹ��Ѵ�.
        case ALTIBASE_NLS_CHARACTERSET:
            (void)getString( ulnDbcGetNlsCharsetString(sDbc), aValPtr, aValLen, aIndPtr);
            break;

        case ALTIBASE_NLS_NCHAR_CHARACTERSET:
            // ���ų� ĳ���� ���� �ǹ��Ѵ�.
            (void)getString( ulnDbcGetNlsNcharCharsetString(sDbc), aValPtr, aValLen, aIndPtr);
            break;

        case SQL_ATTR_ACCESS_MODE:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrAccessMode;
            break;

        case SQL_ATTR_PORT:
            *(acp_uint32_t*)aValPtr = sDbc->mPortNumber;
            break;

        case SQL_ATTR_AUTOCOMMIT:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrAutoCommit;
            break;

        case SQL_ATTR_CONNECTION_TIMEOUT:
            *(acp_uint32_t*)aValPtr = ulnDbcGetConnectionTimeout(sDbc);
            break;

        case SQL_ATTR_QUERY_TIMEOUT:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrQueryTimeout;
            break;

        case SQL_ATTR_CURRENT_CATALOG:
            (void)getString( ulnDbcGetCurrentCatalog(sDbc), aValPtr, aValLen, aIndPtr );
            break;

        case SQL_ATTR_DISCONNECT_BEHAVIOR:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrDisconnect;
            break;

        case SQL_ATTR_ENLIST_IN_DTC:
            /*
             * O1S02
             */
            ulnError(&sFnContext,
                     ulERR_IGNORE_OPTION_VALUE_CHANGED,
                     "SQL_ATTR_ENLIST_IN_XA");
            break;

        case SQL_ATTR_ENLIST_IN_XA:
            /*
             * O1S02
             */
            ulnError(&sFnContext,
                     ulERR_IGNORE_OPTION_VALUE_CHANGED,
                     "SQL_ATTR_ENLIST_IN_XA");
            break;

        case SQL_ATTR_LOGIN_TIMEOUT:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrLoginTimeout;
            break;

        case SQL_ATTR_ODBC_CURSORS:
            *(acp_uint32_t*)aValPtr  =  sDbc->mAttrOdbcCursors;
            break;

        case SQL_ATTR_PACKET_SIZE:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrPacketSize;
            break;

        case ALTIBASE_STACK_SIZE: //BUGBUG request to server should or no ???
            *(acp_uint32_t*)aValPtr = sDbc->mAttrStackSize;
            break;

        case SQL_ATTR_QUIET_MODE:
            /*
             * O1S02
             */
            ulnError(&sFnContext,
                     ulERR_IGNORE_OPTION_VALUE_CHANGED,
                     "SQL_ATTR_QUIET_MODE");

            *(acp_uint32_t*)aValPtr = (acp_uint32_t)sDbc->mAttrQuietMode;
            break;

        case SQL_ATTR_TRACE:
            /*
             * O1S02
             */
            ulnError(&sFnContext,
                     ulERR_IGNORE_OPTION_VALUE_CHANGED,
                     "SQL_ATTR_TRACE");

            *(acp_uint32_t*)aValPtr = sDbc->mAttrTrace;
            break;

        case SQL_ATTR_TRACEFILE:
            /*
             * O1S02
             */
            ulnError(&sFnContext,
                     ulERR_IGNORE_OPTION_VALUE_CHANGED,
                     "SQL_ATTR_TRACEFILE");

            (void)getString( sDbc->mAttrTracefile, aValPtr, aValLen, aIndPtr);
            break;

        case SQL_ATTR_TRANSLATE_LIB:
            /*
             * O1S02
             */
            ulnError(&sFnContext,
                     ulERR_IGNORE_OPTION_VALUE_CHANGED,
                     "SQL_ATTR_TRANSLATE_LIB");

            (void)getString( sDbc->mAttrTranslateLib, aValPtr, aValLen, aIndPtr);
            break;

        case SQL_ATTR_TRANSLATE_OPTION:
            /*
             * O1S02
             */
            ulnError(&sFnContext,
                     ulERR_IGNORE_OPTION_VALUE_CHANGED,
                     "SQL_ATTR_TRANSLATE_OPTION");

            *(acp_uint32_t*)aValPtr = sDbc->mAttrTranslateOption;
            break;

        case SQL_ATTR_TXN_ISOLATION:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrTxnIsolation;
            break;

        case SQL_ATTR_ODBC_VERSION:
            *(acp_uint32_t*)aValPtr = sDbc->mOdbcVersion;
            break;

        case SQL_ATTR_CONNECTION_POOLING: /**< Environment KeekUp BUGBUG */
            *(acp_uint32_t*)aValPtr = sDbc->mAttrConnPooling;
            break;

        case SQL_ATTR_CONNECTION_DEAD:

            /* bug-29379
             * sql_attr_connection_dead promptly detects dead conn
             * before: ���� ����� ����������� ��ȯ
             * after : ���� ������¸� Ȯ���Ͽ� ��� ��ȯ
             */
            if (ulnDbcIsConnected(sDbc) == ACP_FALSE)
            {
                *(acp_uint32_t*)aValPtr = SQL_TRUE;
            }
            /* check current connection state */
            else
            {
                sIsClosed = ACP_FALSE;
                cmiCheckLink((cmiLink *)sDbc->mLink, &sIsClosed);
                if (sIsClosed == ACP_TRUE)
                {
                    *(acp_uint32_t*)aValPtr = SQL_TRUE;
                }
                else
                {
                    *(acp_uint32_t*)aValPtr = SQL_FALSE;
                }
            }
            break;

        case SQL_ATTR_ASYNC_ENABLE:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrAsyncEnable;
            break;

        case SQL_ATTR_METADATA_ID:
            ACI_RAISE(LABEL_NOT_IMPLEMENTED);
            break;

        case ALTIBASE_EXPLAIN_PLAN:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrExplainPlan;
            break;

        case ALTIBASE_OPTIMIZER_MODE:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrOptimizerMode;
            break;

        /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
        case ALTIBASE_DDL_TIMEOUT:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrDdlTimeout;
            break;

        case ALTIBASE_UTRANS_TIMEOUT:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrUtransTimeout;
            break;

        case ALTIBASE_FETCH_TIMEOUT:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrFetchTimeout;
            break;

        case ALTIBASE_IDLE_TIMEOUT:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrIdleTimeout;
            break;

        case ALTIBASE_HEADER_DISPLAY_MODE:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrHeaderDisplayMode;
            break;

        case SQL_ATTR_LONGDATA_COMPAT:
            *(acp_uint32_t*)aValPtr = (ulnDbcGetLongDataCompat(sDbc) == ACP_TRUE) ? SQL_TRUE : SQL_FALSE;
            break;

        case ALTIBASE_XA_NAME:
            (void)getString(sDbc->mXaName, aValPtr, aValLen, aIndPtr);
            break;

        /* proj-1538 ipv6 */
        case ALTIBASE_PREFER_IPV6:
            *(acp_uint32_t*)aValPtr =
                (sDbc->mAttrPreferIPv6 == ACP_TRUE) ? SQL_TRUE : SQL_FALSE;
            break;

        /* BUG-31144 */
        case ALTIBASE_MAX_STATEMENTS_PER_SESSION:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrMaxStatementsPerSession;
            break;

        /* PROJ-2209 DBTIMEZONE */
        case ALTIBASE_TIME_ZONE:
            (void)getString( sDbc->mTimezoneString, aValPtr, aValLen, aIndPtr );
            break;

        /* PROJ-2047 Strengthening LOB - LOBCACHE */
        case ALTIBASE_LOB_CACHE_THRESHOLD:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrLobCacheThreshold;
            break;

        /* BUG-36548 Return code of client functions should be differed by ODBC version */
        case ALTIBASE_ODBC_COMPATIBILITY:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrOdbcCompatibility;
            break;

        /* BUG-36729 Connection attribute will be added to unlock client mutex by force */
        case ALTIBASE_FORCE_UNLOCK:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrForceUnlock;
            break;

        /* BUG-34447 isql SET NUMF[ORMAT] fmt */
        case ALTIBASE_SESSION_ID:
            *(acp_uint32_t*)aValPtr = sDbc->mSessionID;
            break;

        /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
        case ALTIBASE_SOCK_RCVBUF_BLOCK_RATIO:
            *(acp_uint32_t *)aValPtr = ulnDbcGetAttrSockRcvBufBlockRatio(sDbc);
            break;

        /* BUG-44271 */
        case ALTIBASE_SOCK_BIND_ADDR:
            (void)getString( sDbc->mSockBindAddr, aValPtr, aValLen, aIndPtr );
            break;

        /* BUG-45286 */
        case ALTIBASE_PDO_DEFER_PROTOCOLS:
            *(acp_uint32_t*)aValPtr = sDbc->mAttrPDODeferProtocols;
            break;

        default:
            ACI_RAISE(ERR_HY092);
    }

    /*
     * Exit
     */
    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(ERR_HY092);
    {
        ulnError(&sFnContext, ulERR_ABORT_INVALID_ATTR_OPTION, aAttrCode);
    }

    ACI_EXCEPTION(ERR_IH000);
    {
        ULN_FNCONTEXT_SET_RC(&sFnContext, SQL_INVALID_HANDLE);
    }

    ACI_EXCEPTION(LABEL_NOT_IMPLEMENTED)
    {
        ulnError(&sFnContext, ulERR_ABORT_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
    }

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}



ACI_RC ulnCallbackDBPropertyGetResult(cmiProtocolContext *aProtocolContext,
                                      cmiProtocol        *aProtocol,
                                      void               *aServiceSession,
                                      void               *aUserContext)
{
    cmiProtocolContext        *sCtx       = aProtocolContext;
    ulnFnContext              *sFnContext = (ulnFnContext *)aUserContext;
    ulnDbc                    *sDbc       = sFnContext->mHandle.mDbc;
    acp_uint32_t               sLen       = ACP_UINT32_MAX;
    acp_uint16_t               sPropertyID;
    acp_uint16_t               sOrgCursor;
    acp_uint32_t               sPropertyValue4; /* BUG-39817 */

    ACP_UNUSED(aProtocolContext);
    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    CMI_RD2(sCtx, &sPropertyID);

    sOrgCursor = sCtx->mReadBlock->mCursor;

    /*
     * Note : uln �� function context �ȿ��� �������� property get request �� ���� �� ��������,
     *        function context �� object �� stmt �� ��쵵 �����ؾ� �Ѵ�.
     *        Ư��, date format �� �����ͼ� ����ȭ��Ű�� �κп��� �׷���.
     *
     *        �ȱ׷���, �ڵ� Ÿ���� dbc �� �ƴϹǷ� ������ ���µ�,
     *        �� ������ -_-;;;; 08s02, communication failure �� ���´�.
     *
     * BUGBUG : �ݹ��Լ��� ACI_FAILURE �� �����ϸ� �ȵȴ�.
     *          ������ �����ϰ�, ACI_SUCCESS �� �����ؾ� �Ѵ�.
     *
     *          �׷��� �� �Լ��� �����ϴ�, �����ڵ� �����ϴ� �κ��� ����.
     *          ������ �����ڵ带 �����ϵ��� �ҽ��� �����ؾ� �Ѵ�.
     *          �ϴ�, ACI_FAILURE �� �����ϵ��� �� ���·� ������ communication fail �� �����ϵ���
     *          �� �ξ��ٰ� �ҽ� ���� �Ŀ� ACI_SUCCESS �� �����ϵ��� ��ģ��.
     */
    if(ULN_OBJ_GET_TYPE(sDbc) == ULN_OBJ_TYPE_STMT)
    {
        sDbc = ((ulnStmt *)sDbc)->mParentDbc;
    }

    ACI_TEST(ULN_OBJ_GET_TYPE(sDbc) != ULN_OBJ_TYPE_DBC);

    switch(sPropertyID)
    {
        case ULN_PROPERTY_AUTOCOMMIT              :
            CMI_RD1(sCtx, sDbc->mAttrAutoCommit);
            break;

        case ULN_PROPERTY_EXPLAIN_PLAN            :
            CMI_RD1(sCtx, sDbc->mAttrExplainPlan);
            break;

        case ULN_PROPERTY_ISOLATION_LEVEL         :
            CMI_RD4(sCtx, &sPropertyValue4);

            switch (sPropertyValue4) /* BUG-39817 */
            {
                case ULN_SERVER_ISOLATION_LEVEL_READ_COMMITTED:
                    sDbc->mAttrTxnIsolation = SQL_TXN_READ_COMMITTED;
                    break;

                case ULN_SERVER_ISOLATION_LEVEL_REPEATABLE_READ:
                    sDbc->mAttrTxnIsolation = SQL_TXN_REPEATABLE_READ;
                    break;

                case ULN_SERVER_ISOLATION_LEVEL_SERIALIZABLE:
                    sDbc->mAttrTxnIsolation = SQL_TXN_SERIALIZABLE;
                    break;

                default:
                    ACI_TEST(1);
            }
            break;

        case ULN_PROPERTY_OPTIMIZER_MODE          :
            CMI_RD4(sCtx, &sDbc->mAttrOptimizerMode);
            break;

        case ULN_PROPERTY_HEADER_DISPLAY_MODE     :
            CMI_RD4(sCtx, &sDbc->mAttrHeaderDisplayMode);
            break;

        case ULN_PROPERTY_STACK_SIZE              :
            CMI_RD4(sCtx, &sDbc->mAttrStackSize);
            break;

        case ULN_PROPERTY_IDLE_TIMEOUT            :
            CMI_RD4(sCtx, &sDbc->mAttrIdleTimeout);
            break;


        /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
        case ULN_PROPERTY_DDL_TIMEOUT           :
            CMI_RD4(sCtx, &sDbc->mAttrDdlTimeout);
            break;

        case ULN_PROPERTY_QUERY_TIMEOUT           :
            CMI_RD4(sCtx, &sDbc->mAttrQueryTimeout);
            break;

        case ULN_PROPERTY_FETCH_TIMEOUT           :
            CMI_RD4(sCtx, &sDbc->mAttrFetchTimeout);
            break;

        case ULN_PROPERTY_UTRANS_TIMEOUT          :
            CMI_RD4(sCtx, &sDbc->mAttrUtransTimeout);
            break;

        case ULN_PROPERTY_DATE_FORMAT             :
            CMI_RD4(sCtx, &sLen);
            ACI_TEST(ulnDbcAttrMem( &sDbc->mDateFormat, sLen) != ACI_SUCCESS);
            CMI_RCP(sCtx, sDbc->mDateFormat, sLen);
            break;
        // fix BUG-18971
        case ULN_PROPERTY_SERVER_PACKAGE_VERSION  :
            CMI_RD4(sCtx, &sLen);
            ACI_TEST(ulnDbcAttrMem( &sDbc->mServerPackageVer, sLen) != ACI_SUCCESS);
            CMI_RCP(sCtx, sDbc->mServerPackageVer, sLen);
            break;

        case ULN_PROPERTY_CLIENT_PACKAGE_VERSION  :
        case ULN_PROPERTY_CLIENT_PROTOCOL_VERSION :
        case ULN_PROPERTY_CLIENT_PID              :
        case ULN_PROPERTY_CLIENT_TYPE             :
        case ULN_PROPERTY_NLS                     :
        case ULN_PROPERTY_NLS_NCHAR_LITERAL_REPLACE :   /* PROJ-1579 NCHAR */
        case ULN_PROPERTY_TIME_ZONE: /* PROJ-2209 DBTIMEZONE */
            ACI_TEST(1);
            break;

        case ULN_PROPERTY_NLS_CHARACTERSET        :     /* PROJ-1579 NCHAR */
            CMI_RD4(sCtx, &sLen);
            ACI_TEST(ulnDbcAttrMem( &sDbc->mNlsCharsetString, sLen) != ACI_SUCCESS);
            CMI_RCP(sCtx, sDbc->mNlsCharsetString, sLen);

            if (sDbc->mNlsCharsetString != NULL)
            {
                ACI_TEST(mtlModuleByName( (const mtlModule **)&(sDbc->mCharsetLangModule),
                                          sDbc->mNlsCharsetString,
                                          acpCStrLen(sDbc->mNlsCharsetString, ACP_SINT32_MAX) )
                         != ACI_SUCCESS);
            }

            break;

        case ULN_PROPERTY_NLS_NCHAR_CHARACTERSET  :     /* PROJ-1579 NCHAR */
            CMI_RD4(sCtx, &sLen);
            ACI_TEST(ulnDbcAttrMem( &sDbc->mNlsNcharCharsetString, sLen) != ACI_SUCCESS);
            CMI_RCP(sCtx, sDbc->mNlsNcharCharsetString, sLen);

            if(sDbc->mNlsNcharCharsetString != NULL)
            {
                ACI_TEST(mtlModuleByName( (const mtlModule **)&(sDbc->mNcharCharsetLangModule),
                                          sDbc->mNlsNcharCharsetString,
                                          acpCStrLen(sDbc->mNlsNcharCharsetString, ACP_SINT32_MAX) )
                         != ACI_SUCCESS);
            }
            break;

        case ULN_PROPERTY_ENDIAN              :
            CMI_RD1(sCtx, sDbc->mIsSameEndian);
#if defined(ENDIAN_IS_BIG_ENDIAN)

#else
            sDbc->mIsSameEndian = !(sDbc->mIsSameEndian);
#endif
            break;

        /* BUG-31144 */
        case ULN_PROPERTY_MAX_STATEMENTS_PER_SESSION:
            CMI_RD4(sCtx, &sDbc->mAttrMaxStatementsPerSession);
            break;

        /* PROJ-2047 Strengthening LOB - LOBCACHE */
        case ULN_PROPERTY_LOB_CACHE_THRESHOLD:
            CMI_RD4(sCtx, &sDbc->mAttrLobCacheThreshold);
            break;

        /*
         * Warning : ������ �߰��Ǵ� ����-Ŭ���̾�Ʈ ������Ƽ�� �ݵ��
         *           ulnSetConnectAttrOff()�� �߰��Ǿ�� �Ѵ�.
         */

        default:
            ACI_TEST(1);
    }

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;

    sCtx->mReadBlock->mCursor = sOrgCursor;

    switch(sPropertyID)
    {
        case ULN_PROPERTY_AUTOCOMMIT              :
        case ULN_PROPERTY_EXPLAIN_PLAN            :
        case ULN_PROPERTY_ENDIAN                  :
            sCtx->mReadBlock->mCursor += 1;
            break;

        case ULN_PROPERTY_ISOLATION_LEVEL         :
        case ULN_PROPERTY_OPTIMIZER_MODE          :
        case ULN_PROPERTY_HEADER_DISPLAY_MODE     :
        case ULN_PROPERTY_STACK_SIZE              :
        case ULN_PROPERTY_IDLE_TIMEOUT            :
        case ULN_PROPERTY_QUERY_TIMEOUT           :
        case ULN_PROPERTY_FETCH_TIMEOUT           :
        case ULN_PROPERTY_UTRANS_TIMEOUT          :
        case ULN_PROPERTY_DDL_TIMEOUT             :
        case ULN_PROPERTY_MAX_STATEMENTS_PER_SESSION:
        case ULN_PROPERTY_LOB_CACHE_THRESHOLD     :  /* BUG-36966 */
            sCtx->mReadBlock->mCursor += 4;
            break;

        case ULN_PROPERTY_DATE_FORMAT             :
        case ULN_PROPERTY_SERVER_PACKAGE_VERSION  :
        case ULN_PROPERTY_NLS_CHARACTERSET        :
        case ULN_PROPERTY_NLS_NCHAR_CHARACTERSET  :
            CMI_RD4(sCtx, &sLen);
            sCtx->mReadBlock->mCursor += sLen;
            break;

        default:
            break;
    }

    return ACI_FAILURE;
}