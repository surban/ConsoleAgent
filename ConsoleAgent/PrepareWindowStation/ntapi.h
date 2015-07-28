#pragma once

#include <SDKDDKVer.h>
#include <windows.h>



// status
#define STATUS_SUCCESS			0x00000000L
#define STATUS_BUFFER_TOO_SMALL	0xC0000023L

// object flags
#define OBJ_INHERIT             0x00000002L
#define OBJ_PERMANENT           0x00000010L
#define OBJ_EXCLUSIVE           0x00000020L
#define OBJ_CASE_INSENSITIVE    0x00000040L
#define OBJ_OPENIF              0x00000080L
#define OBJ_OPENLINK            0x00000100L
#define OBJ_KERNEL_HANDLE       0x00000200L
#define OBJ_FORCE_ACCESS_CHECK  0x00000400L
#define OBJ_VALID_ATTRIBUTES    0x000007F2L

// directory rights
#define DIRECTORY_QUERY                 (0x0001)
#define DIRECTORY_TRAVERSE              (0x0002)
#define DIRECTORY_CREATE_OBJECT         (0x0004)
#define DIRECTORY_CREATE_SUBDIRECTORY   (0x0008)
#define DIRECTORY_ALL_ACCESS			(STANDARD_RIGHTS_REQUIRED | 0xF)


typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWCH   Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
	ULONG Length;
	HANDLE RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG Attributes;
	PVOID SecurityDescriptor;        // Points to type SECURITY_DESCRIPTOR
	PVOID SecurityQualityOfService;  // Points to type SECURITY_QUALITY_OF_SERVICE
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;
typedef CONST OBJECT_ATTRIBUTES *PCOBJECT_ATTRIBUTES;

#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfService = NULL;               \
    }


// functions without exports from ntdll.dll
typedef NTSTATUS(NTAPI *NtOpenDirectoryObject_t)(OUT PHANDLE             DirectoryObjectHandle,
												 IN ACCESS_MASK          DesiredAccess,
												 IN POBJECT_ATTRIBUTES   ObjectAttributes);
extern NtOpenDirectoryObject_t NtOpenDirectoryObject;

typedef NTSTATUS(NTAPI *NtQuerySecurityObject_t)(IN  HANDLE               Handle,
												 IN  SECURITY_INFORMATION SecurityInformation,
												 OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
												 IN  ULONG                Length,
												 OUT PULONG               LengthNeeded);
extern NtQuerySecurityObject_t NtQuerySecurityObject;

typedef NTSTATUS(NTAPI *NtSetSecurityObject_t)(IN HANDLE               ObjectHandle,
											   IN SECURITY_INFORMATION SecurityInformationClass,
											   IN PSECURITY_DESCRIPTOR DescriptorBuffer);
extern NtSetSecurityObject_t NtSetSecurityObject;

typedef NTSTATUS(NTAPI *NtClose_t)(IN HANDLE Handle);
extern NtClose_t NtClose;


// helper functions
NTSTATUS RtlUnicodeStringInit(OUT PUNICODE_STRING  DestinationString,
							  IN  PCWSTR		   pszSrc);

void ImportFunctions();
