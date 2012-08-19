#ifndef SRL_PROTOCOL_H_
#define SRL_PROTOCOL_H_

/*
 * Sereal Protocol version 1, see constants below docs.
 *
 * Generally speaking, structures are serialized depth-first and each item
 * is preceded/defined by a 1-byte control character, see table below.
 * All packets of Sereal data must be preceded by a header. The header structure
 * is as follows:
 * Bytes 1-4: Magic string "srlX" where X is the character of the protocol version,
 *            in binary. So for protocol version 1, the magic string is "srl\x01".
 * Next: A varint describing the length of the rest of the header.
 *       Since in protocol version 1, there is currently nothing else in the header,
 *       this varint is always 0 (but that will change). Decoders must be able to
 *       skip the parts of the header that they know nothing about using the total
 *       header length.
 *
 * +--------+-----------------+-------------+--------------------------------------------------------------------------
 *          |Bit              | follow      | Description
 *          | 7 6 5 4 3 2 1 0 | bytes       | 
 * ---------+-----------------+-------------+------------------------------------------------------------
 *          | F 0 0 s x x x x | -           | tiny ints
 * POS      |       0 x x x x | -           | Positive nibble   0 .. 15
 * NEG      |       1 x x x x | -           | Negative nibble -16 .. -1
 * 
 *          | F 0 1 0 0 x x x
 * VARINT   |           0 0 0 | varint      | varint
 * ZIGZAG   |           0 0 1 | varint      | zigzag encoded varint
 * FLOAT    |           0 1 0 |             | float
 * DOUBLE   |           0 1 1 |             | double
 * LDOUBLE  |           1 0 0 |             | long double
 *
 * RESERVED - varint indicates length to skip to if a reader does not handle the type, with 0 meaning "die".
 *          |           1 0 1 | varint      | *reserved*
 *          |           1 1 0 | varint      | *reserved* 
 *          |           1 1 1 | varint      | *reserved*
 *          | F 0 1 0 1 x x x | varint      | *reserved*
 * 
 *          | F 0 1 1 0 y y y |             | Ref/Object(ish)
 * REF      |           0 0 0 |             | scalar ref to next item
 * REUSE    |           0 0 1 | varint      | second/third/... occurrence of a multiply-occurring
 *          |                 |             | substructure (always points at a form of reference)
 * HASH     |           0 1 0 | varint      | hash, varint=length (number of hash keys)
 * ARRAY    |           0 1 1 | varint      | array, varint=length 
 * BLESS    |           1 0 0 | TAG(STR) TAG| bless item into class indicated by TAG
 * BLESSV   |           1 0 1 | varint   TAG| bless item into class indicated by varint *provisional*
 * WEAKEN   |           1 1 0 |             | Following item is a reference and it is weakened
 *          |           1 1 1 | varint      | *reserved*
 * 
 *          | F 0 1 1 1 y y y |             | Miscellaneous        
 * STRING   |           0 0 x | varint      | string, x= utf8 flag, varint=length
 * ALIAS    |           0 1 0 | varint      | alias to previous item indicated by varint
 * COPY     |           0 1 1 | varint      | copy item at offset
 * UNDEF    |           1 0 0 | -           | undef
 * REGEXP   |           1 0 1 | TAG         | next item is a regexp 
 * LIST     |           1 1 0 | tbyte vint pad | numeric array (s=0 unsigned, s=1 signed), varint=length, pad if needed for alignment
 * PAD      |           1 1 1 |             | ignored byte, used by encoder to pad if necessary
 * 
 * ASCII    | F 1 x x x x x x | str         | Short ascii string, x=length
 * 
 * 
 * 
 * F = Flag bit to indicate if the item needs to be tracked during deserialization.
 *     The offset of the tag byte should be remembered, so that it can be referenced
 *     later.
 * 
 * * Dealing with self referential and cyclic structures:
 * While dumping any item with a refcount>1 (including weakrefs) the offset of the tag
 * needs to be tracked. The items F flag is NOT set. Should the item later be encountered
 * during dumping an alias or ref item will be generated with the offset in a varint, and
 * the F flag will be set. 
 * 
 * * Handling objects
 * During dumping the dumper is expected to maintain a mapping of class name to id. Whenever
 * it encounters a new class name it emits a "declare class tag" and then emits the appropriate
 * ref tag with the "is class" bit set.
 *    
 * * Varints
 * Varints are variable length integers where the high bit of each segment (normally a byte
 * but in some cases less) indicates if there is another byte to follow, with the bytes in 
 * least significant order first.
 *
 *
 * TODO: FALSE?
 * TODO: What's with floats?
 */

/* Note: both indicating protocol version, keep in sync */
#define SRL_PROTOCOL_VERSION 1
#define SRL_MAGIC_STRING "srl"

/* Useful constants */
/* See also range constants below for the header byte */
#define SRL_MAX_ASCII_LENGTH       63
#define SRL_POS_MAX_SIZE           15
#define SRL_NEG_MIN_SIZE           16




/* All constants have the F bit unset! */
/* _LOW and _HIGH versions refering to INCLUSIVE range boundaries */
#define SRL_HDR_ASCII           ((char)0b01000000)
#define SRL_HDR_ASCII_LEN_MASK  ((char)0b01111111)

#define SRL_HDR_POS_LOW         ((char)0b00000000) /* 0 */
#define SRL_HDR_POS_HIGH        ((char)0b00001111) /* 15 */
#define SRL_HDR_NEG_HIGH        ((char)0b00010000) /* -1  [16] */
#define SRL_HDR_NEG_LOW         ((char)0b00011111) /* -16 [31]*/

#define STL_HDR_TYPE_MASK       ((char)0b00111111)


#define SRL_HDR_VARINT          ((char)0b00100000)
#define SRL_HDR_ZIGZAG          ((char)0b00100001)
#define SRL_HDR_FLOAT           ((char)0b00100010)
#define SRL_HDR_DOUBLE          ((char)0b00100011)
#define SRL_HDR_LONG_DOUBLE     ((char)0b00100100)


/* Note: Can do reserved check with a range now, but as we start using
 *       them, might have to explicit == check later. */
#define SRL_HDR_RESERVED_LOW    ((char)0b00100101)
#define SRL_HDR_RESERVED_HIGH   ((char)0b00101111)

#define SRL_HDR_REF             ((char)0b00110000) /* scalar ref to next item */
#define SRL_HDR_REUSE           ((char)0b00110001) /* second/third/... occurrence of a multiply-occurring
                                                  * substructure (always points at a form of reference) */
#define SRL_HDR_HASH            ((char)0b00110010)
#define SRL_HDR_ARRAY           ((char)0b00110011)
#define SRL_HDR_BLESS           ((char)0b00110100)
#define SRL_HDR_BLESSV          ((char)0b00110101) /* provisional */
#define SRL_HDR_WEAKEN          ((char)0b00110110)
#define SRL_HDR_LIST            ((char)0b00110111)

#define SRL_HDR_STRING          ((char)0b00111000)
#define SRL_HDR_STRING_UTF8     ((char)0b00111001)
#define SRL_HDR_ALIAS           ((char)0b00111010)
#define SRL_HDR_COPY            ((char)0b00111011)
#define SRL_HDR_UNDEF           ((char)0b00111100)
#define SRL_HDR_REGEXP          ((char)0b00111101)
#define SRL_HDR_LIST            ((char)0b00111110)
#define SRL_HDR_PAD             ((char)0b00111111)
 
/* TODO */

#endif
