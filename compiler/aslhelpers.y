NoEcho('
/******************************************************************************
 *
 * Module Name: aslhelpers.y - helper and option terms
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

')


/*******************************************************************************
 *
 * ASL Helper Terms
 *
 ******************************************************************************/

OptionalBusMasterKeyword
    : ','                                   {$$ = TrCreateLeafOp (
                                                PARSEOP_BUSMASTERTYPE_MASTER);}
    | ',' PARSEOP_BUSMASTERTYPE_MASTER      {$$ = TrCreateLeafOp (
                                                PARSEOP_BUSMASTERTYPE_MASTER);}
    | ',' PARSEOP_BUSMASTERTYPE_NOTMASTER   {$$ = TrCreateLeafOp (
                                                PARSEOP_BUSMASTERTYPE_NOTMASTER);}
    ;

OptionalAccessAttribTerm
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ByteConstExpr             {$$ = $2;}
    | ',' AccessAttribKeyword       {$$ = $2;}
    ;

OptionalAccessSize
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalAccessTypeKeyword   /* Default: AnyAcc */
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_ACCESSTYPE_ANY);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_ACCESSTYPE_ANY);}
    | ',' AccessTypeKeyword         {$$ = $2;}
    ;

OptionalAddressingMode
    : ','                           {$$ = NULL;}
    | ',' AddressingModeKeyword     {$$ = $2;}
    ;

OptionalAddressRange
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' AddressKeyword            {$$ = $2;}
    ;

OptionalBitsPerByte
    : ','                           {$$ = NULL;}
    | ',' BitsPerByteKeyword        {$$ = $2;}
    ;

OptionalBuffer_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' RawDataBufferTerm         {$$ = $2;}
    ;

OptionalByteConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalDecodeType
    : ','                           {$$ = NULL;}
    | ',' DecodeKeyword             {$$ = $2;}
    ;

OptionalDevicePolarity
    : ','                           {$$ = NULL;}
    | ',' DevicePolarityKeyword     {$$ = $2;}
    ;

OptionalDWordConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' DWordConstExpr            {$$ = $2;}
    ;

OptionalEndian
    : ','                           {$$ = NULL;}
    | ',' EndianKeyword             {$$ = $2;}
    ;

OptionalFlowControl
    : ','                           {$$ = NULL;}
    | ',' FlowControlKeyword        {$$ = $2;}
    ;

OptionalIoRestriction
    : ','                           {$$ = NULL;}
    | ',' IoRestrictionKeyword      {$$ = $2;}
    ;

OptionalListString
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_STRING_LITERAL,
                                        ACPI_TO_INTEGER (""));}   /* Placeholder is a NULL string */
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_STRING_LITERAL,
                                        ACPI_TO_INTEGER (""));}   /* Placeholder is a NULL string */
    | ',' TermArg                   {$$ = $2;}
    ;

OptionalLockRuleKeyword     /* Default: NoLock */
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_LOCKRULE_NOLOCK);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_LOCKRULE_NOLOCK);}
    | ',' LockRuleKeyword           {$$ = $2;}
    ;

OptionalMaxType
    : ','                           {$$ = NULL;}
    | ',' MaxKeyword                {$$ = $2;}
    ;

OptionalMemType
    : ','                           {$$ = NULL;}
    | ',' MemTypeKeyword            {$$ = $2;}
    ;

OptionalMinType
    : ','                           {$$ = NULL;}
    | ',' MinKeyword                {$$ = $2;}
    ;

OptionalNameString
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' NameString                {$$ = $2;}
    ;

OptionalNameString_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' NameString                {$$ = $2;}
    ;

OptionalNameString_First
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_ZERO);}
    | NameString                    {$$ = $1;}
    ;

OptionalObjectTypeKeyword
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_OBJECTTYPE_UNK);}
    | ',' ObjectTypeKeyword         {$$ = $2;}
    ;

OptionalParityType
    : ','                           {$$ = NULL;}
    | ',' ParityTypeKeyword         {$$ = $2;}
    ;

OptionalQWordConstExpr
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' QWordConstExpr            {$$ = $2;}
    ;

OptionalRangeType
    : ','                           {$$ = NULL;}
    | ',' RangeTypeKeyword          {$$ = $2;}
    ;

OptionalReadWriteKeyword
    :                                   {$$ = TrCreateLeafOp (
                                            PARSEOP_READWRITETYPE_BOTH);}
    | PARSEOP_READWRITETYPE_BOTH        {$$ = TrCreateLeafOp (
                                            PARSEOP_READWRITETYPE_BOTH);}
    | PARSEOP_READWRITETYPE_READONLY    {$$ = TrCreateLeafOp (
                                            PARSEOP_READWRITETYPE_READONLY);}
    ;

OptionalResourceType_First
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_CONSUMER);}
    | ResourceTypeKeyword           {$$ = $1;}
    ;

OptionalResourceType
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_CONSUMER);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_CONSUMER);}
    | ',' ResourceTypeKeyword       {$$ = $2;}
    ;

/* Same as above except default is producer */
OptionalProducerResourceType
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_PRODUCER);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_RESOURCETYPE_PRODUCER);}
    | ',' ResourceTypeKeyword       {$$ = $2;}
    ;

OptionalSlaveMode
    : ','                           {$$ = NULL;}
    | ',' SlaveModeKeyword          {$$ = $2;}
    ;

OptionalSlaveMode_First
    :                               {$$ = NULL;}
    | SlaveModeKeyword              {$$ = $1;}
    ;

OptionalShareType
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' ShareTypeKeyword          {$$ = $2;}
    ;

OptionalShareType_First
    :                               {$$ = NULL;}
    | ShareTypeKeyword              {$$ = $1;}
    ;

OptionalStopBits
    : ','                           {$$ = NULL;}
    | ',' StopBitsKeyword           {$$ = $2;}
    ;

OptionalStringData
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' StringData                {$$ = $2;}
    ;

OptionalSyncLevel           /* Default: 0 */
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_BYTECONST, 0);}
    | ',' ByteConstExpr             {$$ = $2;}
    ;

OptionalTranslationType_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TranslationKeyword        {$$ = $2;}
    ;

OptionalType
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TypeKeyword               {$$ = $2;}
    ;

OptionalType_Last
    :                               {$$ = NULL;}
    | ','                           {$$ = NULL;}
    | ',' TypeKeyword               {$$ = $2;}
    ;

OptionalUpdateRuleKeyword   /* Default: Preserve */
    :                               {$$ = TrCreateLeafOp (
                                        PARSEOP_UPDATERULE_PRESERVE);}
    | ','                           {$$ = TrCreateLeafOp (
                                        PARSEOP_UPDATERULE_PRESERVE);}
    | ',' UpdateRuleKeyword         {$$ = $2;}
    ;

OptionalWireMode
    : ','                           {$$ = NULL;}
    | ',' WireModeKeyword           {$$ = $2;}
    ;

OptionalWordConstExpr
    : ','                           {$$ = NULL;}
    | ',' WordConstExpr             {$$ = $2;}
    ;

OptionalXferSize
    :                               {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_XFERSIZE_32, 2);}
    | ','                           {$$ = TrCreateValuedLeafOp (
                                        PARSEOP_XFERSIZE_32, 2);}
    | ',' XferSizeKeyword           {$$ = $2;}
    ;
