/*
Implementation by Ronny Van Keer, hereby denoted as "the implementer".

For more information, feedback or questions, please refer to our website:
https://keccak.team/

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#ifndef KTWELVE_H
#define KTWELVE_H

#include <stddef.h>

#define IS_BIG_ENDIAN      4321 /* byte 0 is most significant (mc68k) */
#define IS_LITTLE_ENDIAN   1234 /* byte 0 is least significant (i386) */

#if   defined( __alpha__ ) || defined( __alpha ) || defined( i386 )       || \
      defined( __i386__ )  || defined( _M_I86 )  || defined( _M_IX86 )    || \
      defined( __OS2__ )   || defined( sun386 )  || defined( __TURBOC__ ) || \
      defined( vax )       || defined( vms )     || defined( VMS )        || \
      defined( __VMS )     || defined( _M_X64 )
#  define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN

#elif defined( AMIGA )    || defined( applec )    || defined( __AS400__ )  || \
      defined( _CRAY )    || defined( __hppa )    || defined( __hp9000 )   || \
      defined( ibm370 )   || defined( mc68000 )   || defined( m68k )       || \
      defined( __MRC__ )  || defined( __MVS__ )   || defined( __MWERKS__ ) || \
      defined( sparc )    || defined( __sparc)    || defined( SYMANTEC_C ) || \
      defined( __VOS__ )  || defined( __TIGCC__ ) || defined( __TANDEM )   || \
      defined( THINK_C )  || defined( __VMCMS__ ) || defined( _AIX )       || \
      defined( __s390__ ) || defined( __s390x__ ) || defined( __zarch__ )
#  define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN

#elif defined(__arm__)
# ifdef __BIG_ENDIAN
#  define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
# else
#  define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
# endif
#endif

#define KeccakP1600_implementation_config "all rounds unrolled"
#define KeccakP1600_fullUnrolling

#define KeccakP1600_implementation      "generic 64-bit optimized implementation (" KeccakP1600_implementation_config ")"
#define KeccakP1600_stateSizeInBytes    200
#define KeccakP1600_stateAlignment      8
#define KeccakF1600_FastLoop_supported
#define KeccakP1600_12rounds_FastLoop_supported

#ifdef ALIGN
#undef ALIGN
#endif

#if defined(__GNUC__)
#define ALIGN(x) __attribute__ ((aligned(x)))
#elif defined(_MSC_VER)
#define ALIGN(x) __declspec(align(x))
#elif defined(__ARMCC_VERSION)
#define ALIGN(x) __align(x)
#else
#define ALIGN(x)
#endif

ALIGN(KeccakP1600_stateAlignment) typedef struct KeccakWidth1600_12rounds_SpongeInstanceStruct {
    unsigned char state[KeccakP1600_stateSizeInBytes];
    unsigned int rate;
    unsigned int byteIOIndex;
    int squeezing;
} KeccakWidth1600_12rounds_SpongeInstance;

typedef enum {
    NOT_INITIALIZED,
    ABSORBING,
    FINAL,
    SQUEEZING
} KCP_Phases;
typedef KCP_Phases KangarooTwelve_Phases;

typedef struct {
    KeccakWidth1600_12rounds_SpongeInstance queueNode;
    KeccakWidth1600_12rounds_SpongeInstance finalNode;
    size_t fixedOutputLength;
    size_t blockNumber;
    unsigned int queueAbsorbedLen;
    KangarooTwelve_Phases phase;
} KangarooTwelve_Instance;

/** Extendable ouput function KangarooTwelve.
  * @param  input           Pointer to the input message (M).
  * @param  inputByteLen    The length of the input message in bytes.
  * @param  output          Pointer to the output buffer.
  * @param  outputByteLen   The desired number of output bytes.
  * @param  customization   Pointer to the customization string (C).
  * @param  customByteLen   The length of the customization string in bytes.
  * @return 0 if successful, 1 otherwise.
  */
int KangarooTwelve(const unsigned char *input, size_t inputByteLen, unsigned char *output, size_t outputByteLen, const unsigned char *customization, size_t customByteLen );

/**
  * Function to initialize a KangarooTwelve instance.
  * @param  ktInstance      Pointer to the instance to be initialized.
  * @param  outputByteLen   The desired number of output bytes,
  *                         or 0 for an arbitrarily-long output.
  * @return 0 if successful, 1 otherwise.
  */
int KangarooTwelve_Initialize(KangarooTwelve_Instance *ktInstance, size_t outputByteLen);

/**
  * Function to give input data to be absorbed.
  * @param  ktInstance      Pointer to the instance initialized by KangarooTwelve_Initialize().
  * @param  input           Pointer to the input message data (M).
  * @param  inputByteLen    The number of bytes provided in the input message data.
  * @return 0 if successful, 1 otherwise.
  */
int KangarooTwelve_Update(KangarooTwelve_Instance *ktInstance, const unsigned char *input, size_t inputByteLen);

/**
  * Function to call after all the input message has been input, and to get
  * output bytes if the length was specified when calling KangarooTwelve_Initialize().
  * @param  ktInstance      Pointer to the hash instance initialized by KangarooTwelve_Initialize().
  * If @a outputByteLen was not 0 in the call to KangarooTwelve_Initialize(), the number of
  *     output bytes is equal to @a outputByteLen.
  * If @a outputByteLen was 0 in the call to KangarooTwelve_Initialize(), the output bytes
  *     must be extracted using the KangarooTwelve_Squeeze() function.
  * @param  output          Pointer to the buffer where to store the output data.
  * @param  customization   Pointer to the customization string (C).
  * @param  customByteLen   The length of the customization string in bytes.
  * @return 0 if successful, 1 otherwise.
  */
int KangarooTwelve_Final(KangarooTwelve_Instance *ktInstance, unsigned char *output, const unsigned char *customization, size_t customByteLen);

/**
  * Function to squeeze output data.
  * @param  ktInstance     Pointer to the hash instance initialized by KangarooTwelve_Initialize().
  * @param  data           Pointer to the buffer where to store the output data.
  * @param  outputByteLen  The number of output bytes desired.
  * @pre    KangarooTwelve_Final() must have been already called.
  * @return 0 if successful, 1 otherwise.
  */
int KangarooTwelve_Squeeze(KangarooTwelve_Instance *ktInstance, unsigned char *output, size_t outputByteLen);

#endif
