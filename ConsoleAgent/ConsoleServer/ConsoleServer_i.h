

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.00.0603 */
/* at Fri Jul 03 21:25:25 2015
 */
/* Compiler settings for ConsoleServer.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.00.0603 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __ConsoleServer_i_h__
#define __ConsoleServer_i_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IExec_FWD_DEFINED__
#define __IExec_FWD_DEFINED__
typedef interface IExec IExec;

#endif 	/* __IExec_FWD_DEFINED__ */


#ifndef __Exec_FWD_DEFINED__
#define __Exec_FWD_DEFINED__

#ifdef __cplusplus
typedef class Exec Exec;
#else
typedef struct Exec Exec;
#endif /* __cplusplus */

#endif 	/* __Exec_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_ConsoleServer_0000_0000 */
/* [local] */ 

typedef /* [public][public] */ 
enum __MIDL___MIDL_itf_ConsoleServer_0000_0000_0001
    {
        CONTROLEVENT_C	= 0,
        CONTROLEVENT_BREAK	= ( CONTROLEVENT_C + 1 ) 
    } 	ControlEvent;



extern RPC_IF_HANDLE __MIDL_itf_ConsoleServer_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_ConsoleServer_0000_0000_v0_0_s_ifspec;

#ifndef __IExec_INTERFACE_DEFINED__
#define __IExec_INTERFACE_DEFINED__

/* interface IExec */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IExec;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("C9ABBC7D-280C-4A8C-B6FE-EC95FD50E44A")
    IExec : public IDispatch
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE StartProcess( 
            /* [in] */ BSTR commandLine,
            /* [out] */ BYTE *success,
            /* [out] */ LONGLONG *error) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ReadStdout( 
            /* [out] */ BSTR *data,
            /* [out] */ long *dataLength) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ReadStderr( 
            /* [out] */ BSTR *data,
            /* [out] */ long *dataLength) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE WriteStdin( 
            /* [in] */ BSTR data,
            /* [in] */ long dataLength) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetTerminationStatus( 
            /* [out] */ BYTE *hasTerminated,
            /* [out] */ LONGLONG *exitCode) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SendControl( 
            ControlEvent evt) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Ping( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IExecVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IExec * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IExec * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IExec * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IExec * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IExec * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IExec * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IExec * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        HRESULT ( STDMETHODCALLTYPE *StartProcess )( 
            IExec * This,
            /* [in] */ BSTR commandLine,
            /* [out] */ BYTE *success,
            /* [out] */ LONGLONG *error);
        
        HRESULT ( STDMETHODCALLTYPE *ReadStdout )( 
            IExec * This,
            /* [out] */ BSTR *data,
            /* [out] */ long *dataLength);
        
        HRESULT ( STDMETHODCALLTYPE *ReadStderr )( 
            IExec * This,
            /* [out] */ BSTR *data,
            /* [out] */ long *dataLength);
        
        HRESULT ( STDMETHODCALLTYPE *WriteStdin )( 
            IExec * This,
            /* [in] */ BSTR data,
            /* [in] */ long dataLength);
        
        HRESULT ( STDMETHODCALLTYPE *GetTerminationStatus )( 
            IExec * This,
            /* [out] */ BYTE *hasTerminated,
            /* [out] */ LONGLONG *exitCode);
        
        HRESULT ( STDMETHODCALLTYPE *SendControl )( 
            IExec * This,
            ControlEvent evt);
        
        HRESULT ( STDMETHODCALLTYPE *Ping )( 
            IExec * This);
        
        END_INTERFACE
    } IExecVtbl;

    interface IExec
    {
        CONST_VTBL struct IExecVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IExec_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IExec_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IExec_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IExec_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IExec_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IExec_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IExec_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IExec_StartProcess(This,commandLine,success,error)	\
    ( (This)->lpVtbl -> StartProcess(This,commandLine,success,error) ) 

#define IExec_ReadStdout(This,data,dataLength)	\
    ( (This)->lpVtbl -> ReadStdout(This,data,dataLength) ) 

#define IExec_ReadStderr(This,data,dataLength)	\
    ( (This)->lpVtbl -> ReadStderr(This,data,dataLength) ) 

#define IExec_WriteStdin(This,data,dataLength)	\
    ( (This)->lpVtbl -> WriteStdin(This,data,dataLength) ) 

#define IExec_GetTerminationStatus(This,hasTerminated,exitCode)	\
    ( (This)->lpVtbl -> GetTerminationStatus(This,hasTerminated,exitCode) ) 

#define IExec_SendControl(This,evt)	\
    ( (This)->lpVtbl -> SendControl(This,evt) ) 

#define IExec_Ping(This)	\
    ( (This)->lpVtbl -> Ping(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IExec_INTERFACE_DEFINED__ */



#ifndef __ConsoleServerLib_LIBRARY_DEFINED__
#define __ConsoleServerLib_LIBRARY_DEFINED__

/* library ConsoleServerLib */
/* [version][uuid] */ 


EXTERN_C const IID LIBID_ConsoleServerLib;

EXTERN_C const CLSID CLSID_Exec;

#ifdef __cplusplus

class DECLSPEC_UUID("696511A9-45A2-4CFB-834F-CD134A7FFF5A")
Exec;
#endif
#endif /* __ConsoleServerLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     unsigned long *, unsigned long            , BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserMarshal(  unsigned long *, unsigned char *, BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserUnmarshal(unsigned long *, unsigned char *, BSTR * ); 
void                      __RPC_USER  BSTR_UserFree(     unsigned long *, BSTR * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


