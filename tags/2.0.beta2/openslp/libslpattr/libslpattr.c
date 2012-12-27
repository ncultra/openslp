/*-------------------------------------------------------------------------
 * Copyright (C) 2000 Caldera Systems, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *    Neither the name of Caldera Systems nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * `AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE CALDERA
 * SYSTEMS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *-------------------------------------------------------------------------*/

/** Full attribute parser.
 *
 * @file       libslpattr.c
 * @author     Matthew Peterson, John Calcote (jcalcote@novell.com)
 * @attention  Please submit patches to http://www.openslp.org
 * @ingroup    CommonCodeAttrs
 */

#include "../common/slp_types.h"
#include "../common/slp_debug.h"
#include "libslpattr.h"
#include "libslpattr_internal.h"

/* The strings used to represent non-string variables. */
#define BOOL_TRUE_STR "true"
#define BOOL_TRUE_STR_LEN 4
#define BOOL_FALSE_STR "false"
#define BOOL_FALSE_STR_LEN 5

/* The preamble to every variable. */
#define VAR_PREFIX '('
#define VAR_PREFIX_LEN 1

#define VAR_INFIX '='
#define VAR_INFIX_LEN 1

#define VAR_SUFFIX ')'
#define VAR_SUFFIX_LEN 1

#define VAR_SEPARATOR ','
#define VAR_SEPARATOR_LEN 1

/* The cost of the '(=)' for a non-keyword attribute. */
#define VAR_NON_KEYWORD_SEMANTIC_LEN VAR_PREFIX_LEN + VAR_INFIX_LEN + VAR_SUFFIX_LEN

/* The character with which to escape other characters. */
#define ESCAPE_CHARACTER '\\'

/* The number of characters required to escape a single character. */
#define ESCAPED_LEN 3

/* The preamble for opaques ('\FF') -- this is only used when the attributes
 * are "put on the wire". */
#define OPAQUE_PREFIX "\\FF"
#define OPAQUE_PREFIX_LEN 3


/******************************************************************************
 *                                   Utility
 *****************************************************************************/

/* Tests a character to see if it reserved (as defined in RFC 2608, p11). */
#define IS_RESERVED(x) \
   (((x) == '(' || (x) == ')' || (x) == ',' || (x) == '\\' || (x) == '!' || (x) == '<' \
   || (x) == '=' || (x) == '>' || (x) == '~') || \
   ((((char)0x01 <= (x)) && ((char)0x1F >= (x))) || ((x) == (char)0x7F)))


#define IS_INVALID_VALUE_CHAR(x) \
   IS_RESERVED(x)

#define IS_INVALID_TAG_CHAR(x) \
   (IS_RESERVED(x) \
   || ((x) == '*') || \
   ((x) == (char)0x0D) || ((x) == (char)0x0A) || ((x) == (char)0x09) || ((x) == '_'))

#define IS_VALID_TAG_CHAR(x) !IS_INVALID_TAG_CHAR(x)

/* Tests a character to see if it is in set of known hex characters. */
#define IS_VALID_HEX(x) ( ((x >= '0') && (x <= '9')) /* Number */ \
            || ((x >= 'A') && (x <= 'F')) /* ABCDEF */ \
            || ((x >= 'a') && (x <= 'f')) /* abcdef */ \
            )


/* Tests a character to see if it's a digit. */
#define IS_DIGIT(x)  ((x) >= '0' && (x) <= '9')

/* Find the end of a tag, while checking that said tag is valid. 
 *
 * Returns: Pointer to the character immediately following the end of the tag,
 * or 0 if the tag is improperly formed. 
 */
static char const * find_tag_end(const char * tag)
{
   char const *cur = tag; /* Pointer into the tag for working. */

   while (*cur)
   {
      if (IS_INVALID_TAG_CHAR(*cur))
         break;
      cur++;
   }

   return cur;
}


/* Unescapes an escaped character. 
 *
 * val should point to the escape character starting the value.
 */
static char unescape(char d1, char d2)
{
   SLP_ASSERT(isxdigit((int) d1));
   SLP_ASSERT(isxdigit((int) d2));

   if ((d1 >= 'A') && (d1 <= 'F'))
      d1 = d1 - 'A' + 0x0A;
   else if ((d1 >= 'a') && (d1 <= 'f'))
      d1 = d1 - 'a' + 0x0A;
   else
      d1 = d1 - '0';

   if ((d2 >= 'A') && (d2 <= 'F'))
      d2 = d2 - 'A' + 0x0A;
   else if ((d2 >= 'a') && (d2 <= 'f'))
      d2 = d2 - 'a' + 0x0A;
   else
      d2 = d2 - '0';

   return d2 + (d1 * 0x10);
}


/* Unescapes a string. 
 *
 * Params:
 *  dest -- (IN) Where to write
 *  src -- (IN) Unescaped string
 *  len -- (IN) length of src
 *  unescaped_len -- (OUT) number of characters in unescaped 
 * 
 * Returns: Pointer to start of unescaped string. If an error occurs, 0 is
 * returned (an error consists of an escaped value being truncated).
 */
static char * unescape_into(char * dest, const char * src, size_t len,
      size_t * unescaped_len)
{
   char * start, * write;
   unsigned i;

   SLP_ASSERT(dest);
   SLP_ASSERT(src);

   write = start = dest;

   for (i = 0; i < len; i++, write++)
   {
      if (src[i] == ESCAPE_CHARACTER)
      {
         /*** Check that the characters are legal, and that the value has
          * not been truncated. 
          ***/
         if ((i + 2 < len) && isxdigit((int) src[i + 1]) && isxdigit((int)
                                                                  src[i + 2]))
         {
            *write = unescape(src[i + 1], src[i + 2]);
            i += 2;
         }
         else
            return 0;
      }
      else
         *write = src[i];
   }

   /* Report the unescaped size. */
   if (unescaped_len != 0)
      *unescaped_len = write - start;

   return start;
}

/* Finds the end of a value list, while checking that the value contains legal
 * characters. 
 *
 * PARAMS:
 *  value -- (IN) The start of the value list
 *  value_count -- (OUT) The number of values in the value list
 *  type -- (OUT) The type of the value list
 *  unescaped_size -- (OUT) The size of the unescaped value list. ASSUMING THAT THE LIST IS EITHER OPAQUE OR STRING!
 *  cur -- (OUT) End of the parse.
 * 
 * Returns: 0 on parse error. 1 on valid parse.
 *
 * Note: function not marked "static" so test routine can find it outside
 * of module.
 */
/* static */ int find_value_list_end(char const * value, int * value_count, 
      SLPType * type, size_t * unescaped_len, char const * *cur)
{
   enum
   {
      START_VAL,
      /* We're at the start of a value */
      IN_VAL /* We're in a val. */
   } state = START_VAL; /* The state of the current read. */

   enum
   {
      TYPE_UNKNOWN                              = -12,
      /* Could be anything. */
      TYPE_INT                                  = SLP_INTEGER,
      /* Either an int or a string. */
      TYPE_OPAQUE                               = SLP_OPAQUE,
      /* Definitely an opaque. */
      TYPE_STR                                  = SLP_STRING,
      /* Definitely a string. */
      TYPE_BOOL                                 = SLP_BOOLEAN /* A bool, but it could be a string. */
   } type_guess = TYPE_UNKNOWN; /* The current possible values for the type. */

   *value_count = 1;
   *unescaped_len = 0;
   *cur = value;


   while (**cur)
   {
      if (**cur == '\\')
      {
         if (state == START_VAL)
         {
            int this_type = TYPE_OPAQUE;	/* may be either a string or an opaque */

            (*cur)++;
            if (!IS_VALID_HEX(**cur))
               return 0;	/* not even a valid escape sequence */

            /*** Test if we're starting an opaque. ***/
            if (((**cur) != 'F') && ((**cur) != 'f'))
            {
               /* Opaque must start with \FF, so this must be a string */
               this_type = TYPE_STR;
            }

            (*cur)++;
            if (!IS_VALID_HEX(**cur))
               return 0;	/* not even a valid escape sequence */

            /*** Test if we're starting an opaque. ***/
            if ((this_type == TYPE_OPAQUE) && ((**cur) != 'F') && ((**cur) != 'f'))
            {
               /* Opaque must start with \FF, so this must be a string */
               this_type = TYPE_STR;
            }

            /*** We're starting a string or an opaque. Ensure proper typing. ***/
            if (type_guess == TYPE_UNKNOWN)
               type_guess = this_type;
            else if (((this_type != TYPE_OPAQUE) && (type_guess == TYPE_OPAQUE)) ||
                     ((this_type == TYPE_OPAQUE) && (type_guess != TYPE_OPAQUE)))
            {
               /* An opaque is mixed in with non-opaques. Fail. */
               return 0;
            }
            else if (this_type == TYPE_STR)
               /* This value can't be a more specific type, so the whole attribute list can't either */
               type_guess = TYPE_STR;
         }
         else
         {
            /*** We're in the middle of a value. ***/
            /** Check that next two characters are valid. **/
            (*cur)++;
            if (!IS_VALID_HEX(**cur))
               return 0;

            (*cur)++;
            if (!IS_VALID_HEX(**cur))
               return 0;

            (*unescaped_len)++;
         }

         state = IN_VAL;
      }
      else if (**cur == VAR_SEPARATOR)
      {
         /* A separator. */
         /** Check for empty values. **/
         if (state != IN_VAL)
         {
            return 0; /* ERROR! commas side-by-side. */
         }
         state = START_VAL;

         /** Type check. **/
         if (type_guess == TYPE_BOOL)
         {
            /* Bools can only have _one_ value. */
            /* Devolve to string. */
            type_guess = TYPE_STR;
         }
         (*value_count)++;
      }
      else if (**cur == VAR_SUFFIX)
      {
         /* Nous sommes fini. */
         break;
      }
      else if (IS_INVALID_VALUE_CHAR(**cur))
      {
         /* Bad char. */
         return 0;
      }
      else
      {
            /* Normal case */
            /*** Ensure that the character is consistent with its type. ***/
            /** Opaque. **/
         if (type_guess == TYPE_OPAQUE)
         {
            /* Type error! The string starts with a \FF, but has a bare character somewhere following. */
            return 0;
         }
            /** Int. **/
         else if (type_guess == TYPE_INT)
         {
            if (!(IS_DIGIT(**cur) || (state == START_VAL && **cur == '-')))
            {
               /* Devolve to a string. */
               type_guess = TYPE_STR;
            }
         }
         /** Bool. **/
         else if (type_guess == TYPE_BOOL)
         {
            if (*unescaped_len < BOOL_TRUE_STR_LEN
                  && **cur == BOOL_TRUE_STR[*unescaped_len])
            {
               /* Do nothing. It's valid. */
            }
            else if (*unescaped_len < BOOL_FALSE_STR_LEN
                  && **cur == BOOL_FALSE_STR[*unescaped_len])
            {
               /* Do nothing. It's also valid. */
            }
            else
            {
               /* Devolve to a string. */
               type_guess = TYPE_STR;
            }
         }
            /** Unknown. **/
         else if (type_guess == TYPE_UNKNOWN)
         {
            if (IS_DIGIT(**cur) || (state == START_VAL && **cur == '-'))
               type_guess = TYPE_INT;
            else if (state == START_VAL
                  && (BOOL_TRUE_STR[0] == **cur || **cur == BOOL_FALSE_STR[0]))
               type_guess = TYPE_BOOL;
            else
               type_guess = TYPE_STR;
         }

            (*unescaped_len)++;
            state = IN_VAL;
      }

      (*cur)++;
   }

   *type = type_guess;
   return 1;
}


/* Finds the end of a value, while checking that the value contains legal
 * characters. 
 *
 * Returns: see find_tag_end().
 */
/* static */ char * find_value_end(char * value)
{
   char * cur = value; /* Pointer  into the value string. */

   while (*cur)
   {
      if (IS_INVALID_VALUE_CHAR(*cur) && (*cur != '\\'))
         break;
      cur++;
   }

   return cur;
}


/* Find the number of digits (base 10) necessary to represent an integer. 
 *
 * Returns the number of digits.
 */
static int count_digits(int number)
{
   int count = 0;

   /***** As far as I recall, log is undefined at one... *****/
   if (number == 1)
      return 1;
   if (number == -1)
      return 2;

   /***** Does it require a negative sign? *****/
   if (number < 0)
      count += 1;

   /***** Count digits *****/
   /*** Can't take the log of zero. ***/
   if (number == 0)
      return 1 + count;

   return count + (int) (ceil(log10((double) labs(number))));
}


/* Verifies that a tag contains only legal characters. */
static SLPBoolean is_valid_tag_len(const char * tag, size_t tag_len)
{
   (void)tag;
   (void)tag_len;
   /* TODO Check. */
   return SLP_TRUE;
}

static SLPBoolean is_valid_tag(const char * tag)
{
   (void)tag;
   /* TODO Check. */
   return SLP_TRUE;
}


/* A boolean expression that tests a character to see if it must be escaped.
 */
#define ATTRIBUTE_RESERVED_TEST(x) \
   (x == '(' || x == ')' || x == ',' || x == '\\' || x == '!' || x == '<' \
    || x == '=' || x == '<' || x == '=' || x == '>' || x == '~' || x == '\0')
/* Tests a character to see if it should be escaped. To be used for everything
 * but opaques. */
static SLPBoolean is_legal_slp_char(char to_test)
{
   if (ATTRIBUTE_RESERVED_TEST(to_test))
      return SLP_FALSE;
   return SLP_TRUE;
}


/* Tests a character to see if it should be escaped for an opaque. */
static SLPBoolean opaque_test(char to_test)
{
   (void)to_test;
   return SLP_FALSE;
}


/* Find the size of an unescaped string (given the escaped string). 
 *
 * Note that len must be positive. 
 *
 * Returns: If positive, the length of the string. If negative, there is some
 * sort of error. 
 */
static size_t find_unescaped_size(const char * str, size_t len)
{
   unsigned i;
   size_t size;

   SLP_ASSERT(len > 0);

   size = len;

   for (i = 0; i < len; i++)
   {
      if (str[i] == ESCAPE_CHARACTER)
      {
         size -= ESCAPED_LEN - 1; /* -1 for the ESCAPE_CHARACTER. */
      }
   }

   return size;
}


/* Find the size of an escaped string. 
 *
 * The "optional" len argument is the length of the string.  If it is
 * negative, the function treats the string as a null-terminated string. If it
 * is positive, the function will read exactly that number of characters. 
 */
static size_t find_escaped_size(const char * str, int len)
{
   int i; /* Index into str. */
   size_t escaped_size; /* The size of the thingy. */

   i = 0;
   escaped_size = 0;
   if (len < 0)
   {
      /***** String is null-terminated. *****/
      for (i = 0; str[i]; i++)
      {
         if (is_legal_slp_char(str[i]) == SLP_TRUE)
            escaped_size++;
         else
            escaped_size += ESCAPED_LEN;
      }
   }
   else
      for (i = 0; i < len; i++)
      {
         if (is_legal_slp_char(str[i]) == SLP_TRUE)
            escaped_size++;
         else
            escaped_size += ESCAPED_LEN;
      }

   return escaped_size;
}


/* Escape a single character. Writes the escaped value into dest, and
 * increments dest. 
 *
 * NOTE: Most of this code is stolen from Dave McCormack's SLPEscape() code.
 * (For OpenSLP). 
 */
static void escape(char to_escape, char ** dest, SLPBoolean(test_fn)(char))
{
   unsigned char hex_digit;
   if (test_fn(to_escape) == SLP_FALSE)
   {
      /* Insert the escape character. */
      *(*dest)++ = ESCAPE_CHARACTER;

      /* Do the first digit. */
      hex_digit = to_escape >> 4;
      *(*dest)++ = hex_digit + (unsigned char)(hex_digit < 10? '0': 'A' - 10);

      /* Do the last digit. */
      hex_digit = to_escape & 0x0F;
      *(*dest)++ = hex_digit + (unsigned char)(hex_digit < 10? '0': 'A' - 10);
   }
   else
      *(*dest)++ = to_escape;
}


/* Escape the passed string (src), writing it into the other passed string
 * (dest). 
 *
 * If the len argument is negative, the src is treated as null-terminated,
 * otherwise that length is escaped. 
 *
 * Returns a pointer to where the addition has ended. 
 */
static char * escape_into(char * dest, char * src, int len)
{
   char * cur_dest; /* Current character in dest. */
   cur_dest = dest;
   if (len < 0)
   {
      /* Treat as null terminated. */
      char * cur_src; /* Current character in src. */

      cur_src = src;
      for (; *cur_src; cur_src++)
         escape(*cur_src, &cur_dest, is_legal_slp_char);
   }
   else
   {
      /* known length. */
      int i; /* Index into src. */
      for (i = 0; i < len; i++)
         escape(src[i], &cur_dest, is_legal_slp_char);
   }
   return cur_dest;
}


/* Special case for escaping opaque strings. Escapes _every_ character in the
 * string. 
 *
 * Note that the size parameter _must_ be defined. 
 * 
 * Returns a pointer to where the addition has ended. 
 */
static char * escape_opaque_into(char * dest, char * src, size_t len)
{
   unsigned i; /* Index into src. */
   char * cur_dest;

   cur_dest = dest; 

   for (i = 0; i < len; i++)
      escape(src[i], &cur_dest, opaque_test);

   return cur_dest;
}

/******************************************************************************
 *
 *                              Individual values
 *
 * What is a value?
 *
 * In SLP an individual attribute can be associated with a list of values. A
 * value is the data associated with a tag. Depending on the type of
 * attribute, there can be zero, one, or many values associated with a single
 * tag. 
 *****************************************************************************/

/* See libslpattr_internal.h for implementation. */

/* Create and initialize a new value. 
 *
 * Params:
 *  extra -- amount of memory to allocate in addition to that needed for the value. This memory can be found at (return_value + sizeof(value_t))
 */
static value_t * value_new(size_t extra)
{
   value_t * value = 0;

   value = (value_t *)malloc(sizeof(value_t) + extra);
   if (value == 0)
      return 0;
   value->next = 0;
   value->data.va_str = 0;

   value->escaped_len = (size_t)(-1);
   value->unescaped_len = (size_t)(-1);
   value->next_chunk = 0;
   value->last_value_in_chunk = value;

   return value;
}


/* Destroy a value. */
static void value_free(value_t * value)
{
   SLP_ASSERT(value->next == 0);
   free(value);
}

/******************************************************************************
 *
 *                              Individual attributes (vars)
 *
 *  What is a var? 
 *
 *  A var is a variable tag that is associated with a list of values. Zero or
 *  more vars are kept in a single SLPAttributes object. Each value stored in
 *  a var is kept in a value struct. 
 *****************************************************************************/

/* See libslpattr_internal.h for struct. */

/* Create a new variable. 
 */
static var_t * var_new(const char * tag, size_t tag_len)
{
   var_t * var; /* Variable being created. */

   SLP_ASSERT(tag != 0);

   /***** Allocate. *****/
   var = (var_t *) malloc(sizeof(var_t) + tag_len + 1); /* +1 for null. */

   if (var == 0)
      return 0;

   /***** Initialize. *****/
   var->next = 0;

   var->tag_len = tag_len;

   var->tag = ((char *) var) + sizeof(var_t);
   memcpy((void *) var->tag, tag, var->tag_len);
   ((char *) (var->tag))[var->tag_len] = 0;

   var->type = -1;

   var->list = 0;
   var->list_size = 0;

   var->modified = SLP_TRUE;

   return var;
}


/* Destroy a value list. Note that the var is not free()'d, only reset. */
static void var_list_destroy(var_t * var)
{
   value_t * value;
   value_t * to_free; /* A pointer back in the value list to free. */

   /***** Check for data. *****/
   if (var->list == 0)
   {
      SLP_ASSERT(var->list_size == 0);
      return;
   }

   /***** Burrow through the value list deleting every chunk of memory behind us as we go. *****/
   value = var->list;
   to_free = 0;
   while (value)
   {
      to_free = value;
      value = value->next_chunk;
      free(to_free);
   }

   /***** Reset the list. *****/
   var->list = 0;
   var->list_size = 0;
}


/* Frees a variable. */
static void var_free(var_t * var)
{
   /***** Sanity check. *****/
   SLP_ASSERT(var->next == 0);

   /***** Free variable. *****/
   var_list_destroy(var);

   free(var);
}


/* Adds a value to a variable. */
static SLPError var_insert(var_t * var, value_t * value, SLPInsertionPolicy policy)
{
   SLP_ASSERT(policy == SLP_ADD || policy == SLP_REPLACE);

   if (value == 0)
      return SLP_OK;

   if (policy == SLP_REPLACE)
      var_list_destroy(var);

   /* Update list. */
   value->last_value_in_chunk->next = var->list;
   value->next_chunk = var->list; /* Update the memory list too. */
   var->list = value;
   var->list_size++;

   /* Set mod flag.*/
   var->modified = SLP_TRUE;

   return SLP_OK;
}

/******************************************************************************
 *
 *                             All the attributes. 
 *
 *****************************************************************************/

/*
 * SLPAttrAlloc() creates and initializes a new instance of SLPAttributes. 
 */
SLPError SLPAttrAlloc(const char * lang, const FILE * template_h,
      SLPBoolean strict, SLPAttributes * slp_attr_h)
{
   struct xx_SLPAttributes ** slp_attr;
   slp_attr = (struct xx_SLPAttributes * *) slp_attr_h;

   /***** Sanity check *****/
   if (strict == SLP_FALSE && template_h != 0)
   {
      /* We can't be strict if we don't have a template. */
      return SLP_PARAMETER_BAD;
   }

   if (strict != SLP_FALSE)
      return SLP_NOT_IMPLEMENTED;
   if (template_h != 0)
      return SLP_NOT_IMPLEMENTED;

   /***** Create. *****/
   (*slp_attr) = (struct xx_SLPAttributes *)
         malloc(sizeof(struct xx_SLPAttributes));

   if (*slp_attr == 0)
      return SLP_MEMORY_ALLOC_FAILED;

   /***** Initialize *****/
   (*slp_attr)->strict = SLP_FALSE;    /* FIXME Add templates. */
   (*slp_attr)->lang = strdup(lang);   /* free()'d in SLPAttrFree(). */
   (*slp_attr)->attrs = 0;
   (*slp_attr)->attr_count = 0;

   /***** Report. *****/
   return SLP_OK;
}


static SLPError attr_destringify(struct xx_SLPAttributes * slp_attr,
      const char * str, SLPInsertionPolicy); 

/* Allocates a new attribute list from a string. */
SLPError SLPAttrAllocStr(const char * lang, const FILE * template_h,
      SLPBoolean strict, SLPAttributes * slp_attr_h, const char * str)
{
   SLPError err;

   err = SLPAttrAlloc(lang, template_h, strict, slp_attr_h);

   if (err != SLP_OK)
      return err;

   err = attr_destringify((struct xx_SLPAttributes *) *slp_attr_h, str,
               SLP_ADD);

   if (err != SLP_OK)
      SLPAttrFree(*slp_attr_h);

   return err;
}


/* Destroys the passed SLPAttributes(). 
 */
void SLPAttrFree(SLPAttributes slp_attr_h)
{
   struct xx_SLPAttributes * slp_attr;
   slp_attr = (struct xx_SLPAttributes *) slp_attr_h;

   /***** Free held resources. *****/
   while (slp_attr->attrs)
   {
      var_t * attr = slp_attr->attrs;
      slp_attr->attrs = attr->next;

      attr->next = 0;

      var_free(attr);
   }

   free(slp_attr->lang);
   slp_attr->lang = 0;

   /***** Free the handle *****/
   free(slp_attr);

   slp_attr = 0;
}


/* Insert a variable into the var list. */
static void attr_add(struct xx_SLPAttributes * slp_attr, var_t * var)
{
   var->next = slp_attr->attrs;
   slp_attr->attrs = var; 

   slp_attr->attr_count++;
}


/* Find a variable by its tag.
 *
 * Returns a 0 if the value could not be found.
 */
var_t * attr_val_find_str(struct xx_SLPAttributes * slp_attr,
      const char * tag, size_t tag_len)
{
   var_t * var;

   var = slp_attr->attrs;
   while (var)
   {
      /* Per RFC 2165 (Section 20.5 para 1), RFC 2608 (Section 6.4 para 3),
       * attr-tags are supposed to be case insensitive.
       * Using strncasecmp() so that comparision of tags are case-insensitive
       * atleast inside the ASCII range.
       */
      if (var->tag_len == (unsigned) tag_len && strncasecmp(var->tag, tag,
                                                      tag_len) == 0)
         return var;
      var = var->next;
   }

   return 0;
}

/* Test a variable's type. Returns SLP_OK if the match is alright, or some
 * other error code (meant to be forwarded to the application) if the match is
 * bad.
 */
static SLPError attr_type_verify(struct xx_SLPAttributes * slp_attr, 
      var_t * var, SLPType type)
{
   (void)slp_attr;

   SLP_ASSERT(var->type != -1); /* Check that it's been set. */
   if (var->type == type)
      return SLP_OK;

   return SLP_TYPE_ERROR; /* FIXME: Check against template. */
}

/******************************************************************************
 *
 *                              Setting attributes
 *
 *****************************************************************************/

/*****************************************************************************/
static SLPError generic_set_val(struct xx_SLPAttributes * slp_attr, const char * tag,
      size_t tag_len, value_t * value, SLPInsertionPolicy policy,
      SLPType attr_type) 
/* Finds and sets the value named in tag.                                    */
/* 
 * slp_attr  - the attr object to add to.
 * tag       - the name of the tag to add to.
 * tag_len   - the length of the tag to add to.
 * value     - the already-allocated value object with fields set
 * policy    - the policy to use when inserting.
 * attr_type - the type of the value being added.
 *****************************************************************************/
{
   var_t * var;   
   /***** Create a new attribute. *****/
   if ((var = attr_val_find_str(slp_attr, tag, tag_len)) == 0)
   {
      /*** Couldn't find a value with this tag. Make a new one. ***/
      var = var_new((char *) tag, tag_len);
      if (var == 0)
         return SLP_MEMORY_ALLOC_FAILED;
      var->type = attr_type;     
      /** Add variable to list. **/
      attr_add(slp_attr, var);
   }
   else
   {
      SLPError err;   
      /*** The attribute already exists. ***/
      /*** Verify type. ***/
      err = attr_type_verify(slp_attr, var, attr_type);   
      if (err == SLP_TYPE_ERROR && policy == SLP_REPLACE)
      {
         var_list_destroy(var); 
         var->type = attr_type;
      }
      else if (err != SLP_OK)
      {
         value_free(value); 
         return err;
      }
   }   
   /***** Set value *****/ 
   var_insert(var, value, policy); 

   return SLP_OK;
}


/* Set a boolean attribute.  */
SLPError SLPAttrSet_bool(SLPAttributes attr_h, const char * tag,
      SLPBoolean val)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   value_t * value = 0;
   int escaped_len;

   /***** Sanity check. *****/
   if (val != SLP_TRUE && val != SLP_FALSE)
      return SLP_PARAMETER_BAD;
   if (is_valid_tag(tag) == SLP_FALSE)
      return SLP_TAG_BAD;

   /***** Set the initial (and only) value. *****/
   /**** Create ****/
   value = value_new(0);
   SLP_ASSERT(value);

   /**** Set escaped information. ****/
   if (val == SLP_TRUE)
      escaped_len = BOOL_TRUE_STR_LEN;
   else
      escaped_len = BOOL_FALSE_STR_LEN;
   value->escaped_len = escaped_len;
   value->data.va_bool = val;


   /**** Set the value and return. ****/
   return generic_set_val(slp_attr, tag, (int) strlen(tag), value,
               SLP_REPLACE, SLP_BOOLEAN);
}


/* Sets a string attribute. */
SLPError SLPAttrSet_str(SLPAttributes attr_h, const char * tag,
      const char * val, SLPInsertionPolicy policy)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   value_t * value;
   size_t unescaped_len;

   /***** Sanity check. *****/
   if (is_valid_tag(tag) == SLP_FALSE)
      return SLP_TAG_BAD;
   if (val == 0)
      return SLP_PARAMETER_BAD;

   /***** Create new value. *****/
   unescaped_len = strlen(val);

   value = value_new(unescaped_len);
   SLP_ASSERT(value);

   /**** Copy data. ****/
   value->data.va_str = ((char *) value) + sizeof(value_t);
   memcpy(value->data.va_str, val, unescaped_len);

   /**** Set lengths. ****/
   value->unescaped_len = unescaped_len;
   value->escaped_len = find_escaped_size(value->data.va_str, (int)unescaped_len);

   return generic_set_val(slp_attr, tag, (int) strlen(tag), value, policy,
               SLP_STRING);
}



/* Sets a keyword attribute. Takes a non-null terminated string. */
SLPError SLPAttrSet_keyw_len(SLPAttributes attr_h, const char * tag,
      size_t tag_len)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;

   /***** Sanity check. *****/
   if (is_valid_tag_len(tag, tag_len) == SLP_FALSE)
      return SLP_TAG_BAD;

   return generic_set_val(slp_attr, tag, tag_len, 0, SLP_REPLACE,
               SLP_KEYWORD);
}

SLPError SLPAttrSet_keyw(SLPAttributes attr_h, const char * tag)
{
   return SLPAttrSet_keyw_len(attr_h, tag, (int) strlen(tag));
}

/* Sets an integer attribute. */
SLPError SLPAttrSet_int(SLPAttributes attr_h, const char * tag, int val,
      SLPInsertionPolicy policy)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   value_t * value;

   /***** Sanity check. *****/
   if (is_valid_tag(tag) == SLP_FALSE)
      return SLP_TAG_BAD;

   /***** Create new value. *****/
   value = value_new(0);
   if (value == 0)
      return SLP_MEMORY_ALLOC_FAILED;

   /**** Set ****/
   value->data.va_int = val;
   value->escaped_len = count_digits(value->data.va_int);
   SLP_ASSERT(value->escaped_len > 0);

   return generic_set_val(slp_attr, tag, (int) strlen(tag), value, policy,
               SLP_INTEGER);
}


/* Set an opaque attribute. */
SLPError SLPAttrSet_opaque(SLPAttributes attr_h, const char * tag,
      const char * val, size_t len, SLPInsertionPolicy policy)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   value_t * value;

   /***** Sanity check. *****/
   if (is_valid_tag(tag) == SLP_FALSE)
      return SLP_TAG_BAD;
   if (val == 0)
      return SLP_PARAMETER_BAD;

   /***** Create a new attribute. *****/
   value = value_new(len);
   if (value == 0)
      return SLP_MEMORY_ALLOC_FAILED;

   memcpy((void *) value->data.va_str, val, len);
   value->unescaped_len = len;
   value->escaped_len = (len * ESCAPED_LEN) + OPAQUE_PREFIX_LEN;

   return generic_set_val(slp_attr, tag, (int) strlen(tag), value, policy,
               SLP_OPAQUE);
}


static SLPError SLPAttrStore(struct xx_SLPAttributes * slp_attr, const char * tag,
      const char * val, size_t len, SLPInsertionPolicy policy);

/* Set an attribute of unknown type. 
 *
 * Note that the policy in this case is a special case: If the policy is 
 * SLP_REPLACE, we delete the current list and replace it with the passed 
 * value. If it's a multivalue list, we replace the current value with the 
 * ENTIRE passed list. 
 *
 * FIXME Should we "elide" whitespace?
 */
SLPError SLPAttrSet_guess(SLPAttributes attr_h, const char * tag,
      const char * val, SLPInsertionPolicy policy)
{
   SLPError err;
   size_t len;
   const char * cur, * end;

   /***** Sanity check. *****/
   if (is_valid_tag(tag) == SLP_FALSE)
      return SLP_TAG_BAD;
   if (val == 0)
      return SLP_PARAMETER_BAD;

   /***** 
    * If we have a replace policy and we're inserting a multivalued list, 
    * the values will clobber each other. Therefore if we have a replace, we 
    * delete the current list, and use an add policy.
    *****/
   if (policy == SLP_REPLACE)
   {
      var_t * var;
      var = attr_val_find_str((struct xx_SLPAttributes *) attr_h, tag,
                  strlen(tag));
      if (var)
         var_list_destroy(var);
   }

   /***** Check for multivalue list. *****/
   cur = val;
   do
   {
      end = strchr(cur, VAR_SEPARATOR);
      if (end == 0)
         len = strlen(cur);
      else
      {
         /*** It's multivalue. ***/
         len = end - cur;
      }

      err = SLPAttrStore((struct xx_SLPAttributes *) attr_h, tag, cur, len,
                  SLP_ADD);
      if (err != SLP_OK)
      {
         /* FIXME Ummm. Should we return or ignore? */
         return err;
      }
      cur = end + VAR_SEPARATOR_LEN;
   }
   while (end);

   /***** Return *****/
   return SLP_OK;
}



/******************************************************************************
 *
 *                              Getting attributes
 *
 *****************************************************************************/

/* Get the value of a boolean attribute. Note that it _cannot_ be multivalued. 
 */
SLPError SLPAttrGet_bool(SLPAttributes attr_h, const char * tag,
      SLPBoolean * val)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   var_t * var;

   var = attr_val_find_str(slp_attr, tag, strlen(tag));

   /***** Check that the tag exists. *****/
   if (var == 0)
      return SLP_TAG_ERROR;

   /* TODO Verify type against template. */

   /***** Verify type. *****/
   if (var->type != SLP_BOOLEAN)
      return SLP_TYPE_ERROR;

   SLP_ASSERT(var->list != 0);

   *val = var->list->data.va_bool;

   return SLP_OK;
}

/* Get the value of a keyword attribute. Since keywords either exist or don't
 * exist, no value is passed out. Instead, if the keyword exists, an SLP_OK is
 * returned, if it doesn't exist, an SLP_TAG_ERROR is returned. If the tag
 * does exist, but is associated with a non-keyword attribute, SLP_TYPE_ERROR
 * is returned. 
 */
SLPError SLPAttrGet_keyw(SLPAttributes attr_h, const char * tag)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   var_t * var;

   var = attr_val_find_str(slp_attr, tag, strlen(tag));

   /***** Check that the tag exists. *****/
   if (var == 0)
      return SLP_TAG_ERROR;

   /* TODO Verify type against template. */

   /***** Verify type. *****/
   if (var->type != SLP_KEYWORD)
      return SLP_TYPE_ERROR;

   SLP_ASSERT(var->list == 0);

   return SLP_OK;
}


/* Get an integer value. Since integer attributes can be multivalued, an array
 * is returned that contains all values corresponding to the given tag. 
 *
 *
 * Note: On success, an array of SLP_INTEGERs is created. It is the caller's
 * responsibility to free the memory returned through val. 
 * 
 * Returns:
 *  SLP_OK
 *    Returned if the attribute is found. The array containing the values is
 *    placed in val, and size is set to the number of values in val. 
 *  SLP_TYPE_ERROR
 *    Returned if the tag exists, but the associated value is not of type
 *    SLP_INTEGER.
 *  SLP_MEMORY_ALLOC_FAILED
 *    Memory allocation failed. 
 */
SLPError SLPAttrGet_int(SLPAttributes attr_h, const char * tag, int ** val,
      size_t * size)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   var_t * var;
   value_t * value;
   int i;

   var = attr_val_find_str(slp_attr, tag, strlen(tag));

   /***** Check that the tag exists. *****/
   if (var == 0)
      return SLP_TAG_ERROR;

   /* TODO Verify type against template. */

   /***** Verify type. *****/
   if (var->type != SLP_INTEGER)
      return SLP_TYPE_ERROR;

   /***** Create return value. *****/
   *size = var->list_size;
   *val = (int *) malloc(sizeof(int) * var->list_size);
   if (*val == 0)
      return SLP_MEMORY_ALLOC_FAILED;

   /***** Set values *****/
   SLP_ASSERT(var->list != 0);
   value = var->list;
   for (i = 0; i < var->list_size; i++, value = value->next)
   {
      SLP_ASSERT(value != 0);
      (*val)[i] = value->data.va_int;
   }

   return SLP_OK;
}

/* Get string values. Since string attributes can be multivalued, an array
 * is returned that contains all values corresponding to the given tag. 
 *
 *
 * Note: On success, an array of SLP_STRINGs is created. It is the caller's
 * responsibility to free the memory returned through val. Note that the array
 * referencing the strings is allocated separately from each string value,
 * meaning that each value must explicitly be deallocated. 
 * 
 * Returns:
 *  SLP_OK
 *    Returned if the attribute is found. The array containing the values is
 *    placed in val, and size is set to the number of values in val. 
 *  SLP_TYPE_ERROR
 *    Returned if the tag exists, but the associated value is not of type
 *    SLP_INTEGER.
 *  SLP_MEMORY_ALLOC_FAILED
 *    Memory allocation failed. 
 */
SLPError SLPAttrGet_str(SLPAttributes attr_h, const char * tag, char *** val,
      size_t * size)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   var_t * var;
   value_t * value;
   int i;

   var = attr_val_find_str(slp_attr, tag, strlen(tag));

   /***** Check that the tag exists. *****/
   if (var == 0)
      return SLP_TAG_ERROR;

   /* TODO Verify type against template. */

   /***** Verify type. *****/
   if (var->type != SLP_STRING)
      return SLP_TYPE_ERROR;

   /***** Create return value. *****/
   *size = var->list_size;
   *val = (char * *) malloc(sizeof(char *) * var->list_size);
   if (*val == 0)
      return SLP_MEMORY_ALLOC_FAILED;

   /***** Set values *****/
   SLP_ASSERT(var->list != 0);
   value = var->list;
   for (i = 0; i < var->list_size; i++, value = value->next)
   {
      SLP_ASSERT(value != 0);
      /* (*val)[i] = strdup(value->data.va_str); */
      (*val)[i] = malloc(value->unescaped_len + 1);
      SLP_ASSERT((*val)[i] != 0);
      memcpy((*val)[i], value->data.va_str, value->unescaped_len);
      (*val)[i][value->unescaped_len] = 0;
   }

   return SLP_OK;
}

/* Get opaque values. Since opaque attributes can be multivalued, an array
 * is returned that contains all values corresponding to the given tag. 
 *
 *
 * Note: On success, an array of SLP_OPAQUEs is created. It is the caller's
 * responsibility to free the memory returned through val. Note that the array
 * referencing the opaques is allocated separately from each opaque struct,
 * and from the corresponding opaque value, meaning that each value must 
 * explicitly be deallocated, as must each opaque struct. 
 * 
 * Returns:
 *  SLP_OK
 *    Returned if the attribute is found. The array containing the values is
 *    placed in val, and size is set to the number of values in val. 
 *  SLP_TYPE_ERROR
 *    Returned if the tag exists, but the associated value is not of type
 *    SLP_INTEGER.
 *  SLP_MEMORY_ALLOC_FAILED
 *    Memory allocation failed. 
 */
SLPError SLPAttrGet_opaque(SLPAttributes attr_h, const char * tag,
      SLPOpaque *** val, size_t * size)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   var_t * var;
   value_t * value;
   int i;

   var = attr_val_find_str(slp_attr, tag, strlen(tag));

   /***** Check that the tag exists. *****/
   if (var == 0)
      return SLP_TAG_ERROR;

   /* TODO Verify type against template. */

   /***** Verify type. *****/
   if (var->type != SLP_OPAQUE)
      return SLP_TYPE_ERROR;

   /***** Create return value. *****/
   *size = var->list_size;
   *val = (SLPOpaque * *) malloc(sizeof(SLPOpaque *) * var->list_size);
   if (*val == 0)
      return SLP_MEMORY_ALLOC_FAILED;

   /***** Set values *****/
   SLP_ASSERT(var->list != 0);
   value = var->list;
   for (i = 0; i < var->list_size; i++, value = value->next)
   {
      SLP_ASSERT(value != 0);
      (*val)[i] = (SLPOpaque *) malloc(sizeof(SLPOpaque));
      if ((*val)[i]->data == 0)
      {
         /* TODO Deallocate everything and return. */
         return SLP_MEMORY_ALLOC_FAILED;
      }
      (*val)[i]->len = value->unescaped_len;
      (*val)[i]->data = (char *) malloc(value->unescaped_len);
      if ((*val)[i]->data == 0)
      {
         /* TODO Deallocate everything and return. */
         return SLP_MEMORY_ALLOC_FAILED;
      }
      memcpy((*val)[i]->data, value->data.va_str, value->unescaped_len);
   }

   return SLP_OK;
}

/* Finds the type of the given attribute. 
 *
 * Returns:
 *  SLP_OK
 *    If the attribute is set. The type is returned through the type
 *    parameter.
 *  SLP_TAG_ERROR
 *    If the attribute is not set. 
 */
SLPError SLPAttrGetType_len(SLPAttributes attr_h, const char * tag,
      size_t tag_len, SLPType * type)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   var_t * var;

   var = attr_val_find_str(slp_attr, tag, tag_len);

   /***** Check that the tag exists. *****/
   if (var == 0)
      return SLP_TAG_ERROR;

   if (type != 0)
      *type = var->type;

   return SLP_OK;
}

SLPError SLPAttrGetType(SLPAttributes attr_h, const char * tag, SLPType * type)
{
   return SLPAttrGetType_len(attr_h, tag, strlen(tag), type);
}

#if 1 /* Jim Meyer's byte allignment code */
/******************************************************************************
 *
 *                          Fix memory alignment
 *
*****************************************************************************/
static char * fix_memory_alignment(char * p)
{
   intptr_t address = (intptr_t)p;
   address = (address + sizeof(intptr_t) - 1) & ~(sizeof(intptr_t) - 1);
   return (char *)address;
}

#endif

/******************************************************************************
 *
 *                          Attribute (En|De)coding 
 *
 *****************************************************************************/


/* Stores a list of serialized attributes. Takes advantage of foreknowledge of 
 * stuff string sizes, etc. 
 *
 * Params:
 *  tag -- (IN) the name of the attribute
 *  tag_len -- (IN) the length of the tag in bytes
 *  attr_start -- (IN) the start of the attribute string
 *  attr_end -- (IN) the end of the attribute string
 *  val_count -- (IN) the number of values in the string
 *  type -- (IN) the type of the string
 *  unescaped_len -- (IN) the length of the unescaped data
 *
 * Returns:
 *   0 - Out of mem.
 *   1 - Success.
 */
static int internal_store(struct xx_SLPAttributes * slp_attr, char const * tag,
      size_t tag_len, char const * attr_start, char const * attr_end,
      int val_count, SLPType type, size_t unescaped_len)
{
   var_t * var;
   size_t block_size;
   char * mem_block; /* Pointer into allocated block. */
   char const *cur_start; /* Pointer into attribute list (start of current data). */
   char const *cur_end; /* Pointer into attribute list (end of current data). */
   value_t * val = 0;
   value_t ** next_val_ptr; /* Pointer from the previous val to the next val. */

   SLP_ASSERT(type == SLP_BOOLEAN
         || type == SLP_STRING
         || type == SLP_OPAQUE
         || type == SLP_INTEGER); /* Ensure that we're dealing with a known type. */


   /***** Allocate the variable. *****/
   var = var_new(tag, tag_len);

   if (var == 0)
      return 0;

   /***** Allocate space for the values. *****/
   block_size = (val_count * sizeof(value_t)) /* Size of each value */
         + unescaped_len /* The size of the unescaped data. */
#if 1 /* Jim Meyer's byte allignment code */
         + val_count * (sizeof(long) - 1); /* Padding */
#endif     
   mem_block = (char *) malloc(block_size);
   if (mem_block == 0)
   {
      free(val);
      return 0;
   }


   /***** Initialize var_t. *****/
   var->type = type;

   var->list_size = val_count;
   var->modified = SLP_TRUE;

   next_val_ptr = &var->list; /* Initialize next_val_ptr */
   *next_val_ptr = 0;


   /***** Initialize values. *****/

   cur_end = cur_start = attr_start;

   while (cur_end < attr_end)
   {
      /**** Find the size of the data. ****/
      cur_end = memchr(cur_start, VAR_SEPARATOR, attr_end - cur_start);

      if (cur_end == 0)
         cur_end = attr_end;

      /**** Create the value. ****/
      *next_val_ptr = val = (value_t *) mem_block;
      val->next = 0; /* Set forward pointer to null. */
      val->next_chunk = 0; /* This is not the first. */
      val->last_value_in_chunk = 0;

      /**** Update kept data. ****/
      next_val_ptr = &val->next; /* Book-keeping for next write. */
      mem_block += sizeof(value_t); /* Move along. */

      /**** FIXME Write the data. ****/
      switch (type)
      {
         case(SLP_BOOLEAN):
            SLP_ASSERT(val_count == 1);

            /* Set value. */
            if (*cur_start == 't' || *cur_start == 'T')
            {
               SLP_ASSERT(strncasecmp(cur_start, BOOL_TRUE_STR, BOOL_TRUE_STR_LEN)
                     == 0); /* Make sure that we do, in actual fact, have the string "true". */
               val->data.va_bool = SLP_TRUE;
               val->escaped_len = BOOL_TRUE_STR_LEN;
            }
            else if (*cur_start == 'f' || *cur_start == 'F')
            {
               SLP_ASSERT(strncasecmp(cur_start, BOOL_FALSE_STR,
                           BOOL_FALSE_STR_LEN)
                     == 0); /* Make sure that we do, in actual fact, have the string "false". */
               val->data.va_bool = SLP_FALSE;
               val->escaped_len = BOOL_FALSE_STR_LEN;
            }
            else
               SLP_ASSERT(0);

            mem_block += val->unescaped_len;

            break;
         case(SLP_INTEGER):
            val->data.va_int = (int) strtol(cur_start, 0, 0);
            val->escaped_len = count_digits(val->data.va_int);
            /* FIXME Check errno. */
            break;
         case(SLP_OPAQUE):
         case(SLP_STRING):
            {
               char * err;

               val->data.va_str = mem_block;
               val->escaped_len = cur_end - cur_start;
               err = unescape_into(val->data.va_str, cur_start,
                           val->escaped_len, &val->unescaped_len);
               if (err == 0)
               {
                  /* FIXME */
               }
               mem_block += val->unescaped_len;
            }
            break;
         default:
            SLP_ASSERT(0); /* Unknown type. */
      }

#if 1 /* Jim Meyer's byte allignment code */
      mem_block = fix_memory_alignment(mem_block);
#endif
      cur_start = cur_end + 1; /* +1 to move past comma. */
   }

   /***** Set pointers for memory management. *****/
   var->list->last_value_in_chunk = val;

   mem_block = mem_block + sizeof(value_t);

   attr_add(slp_attr, var); 
   return 1; /* Success. */
}

/* Iterates across a set of variables. Either by using a given list of tag 
 * names, or looping across all list members. 
 *
 * Params:
 *  slp_attr -- (IN) the attribute list we're working in
 *  tag_cur -- (IN/OUT) the current position in the tag string. Must be 
 *    initialized to point to the start of a tag list. 
 *  tag_end -- (IN/OUT) the end of the current tag. Must be initialized to 
 *    the same value as tag_cur.
 *  var -- (IN/OUT) the variable in question. Must be initialized to the first 
 *    variable in a list of attributes. 
 *
 * Returns:
 *  1 if there are more vars to be iterated over
 *  0 if there are no more vars to be iterated over
 */
static int var_iter(struct xx_SLPAttributes * slp_attr, char ** tag_cur,
      char ** tag_end, var_t ** var)
{
   /*** Get the next var. ***/
   if (*tag_cur)
   {
      /*** We're doing a tag perusal: find next tag name. ***/
      if (**tag_end)
      {
         /* There are more tags to be looked at */
         if (*tag_end != *tag_cur)
         {
            /* This is _not_ our first time thru the loop: push pointers ahead. */
            *tag_cur = *tag_end + 1;
         }

         *tag_end = strchr(*tag_cur, ',');

         /* This is the last tag. */
         if (*tag_end == 0)
            *tag_end = *tag_cur + strlen(*tag_cur);
      }
      else
      {
         /* There are no more tags to be looked at. Stop looping. */
         return 0;
      }

      /*** Get named var. ***/
      *var = attr_val_find_str(slp_attr, *tag_cur, *tag_end - *tag_cur);
      return 1;
   }
   else
   {
      /*** We're getting all vars: get the next var. ***/
      if (*var)
         *var = (*var)->next;
      else
      {
         /* First time into the iterator. */
         *var = slp_attr->attrs;
      }

      if (*var == 0)
      {
         /* Last var. Stop looping. */
         return 0;
      }
      return 1;
   }
}

/* Gets the escaped stringified version of an attribute list. 
 *
 * The string returned must be free()'d by the caller.
 *
 * Params:
 * attr_h -- (IN) Attribute handle to add serialize.
 * tags -- (IN) The tags to serialize. If 0, all tags are serialized. 
 * out_buffer -- (IN/OUT) A buffer to write the serialized string to. If 
 *               (*out_buffer == 0), then a new buffer is allocated by 
 *               the API. 
 * bufferlen -- (IN) The length of the buffer. Ignored if 
 *              (*out_buffer == 0). 
 * count -- (OUT) The size needed/used of out_buffer (includes trailing null).
 * find_delta -- (IN)  If find_delta is set to true, only the attributes that 
 *               have changed since the last serialize are updated.
 * 
 * Returns:
 * SLP_OK -- Serialization occured. 
 * SLP_BUFFER_OVERFLOW -- If (*out_buffer) is defined, but bufferlen is 
 *                    smaller than the amount of memory necessary to serialize 
 *                    the attr. list.
 * SLP_MEMORY_ALLOC_FAILED -- Ran out of memory. 
 */
SLPError SLPAttrSerialize(SLPAttributes attr_h,
      const char * tags /* 0 terminated */,
      char ** out_buffer /* Where to write. if *out_buffer == 0, space is alloc'd */,
      size_t bufferlen, /* Size of buffer. */
      size_t * count, /* Bytes needed/written. */
      SLPBoolean find_delta)
{
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;
   var_t * var; /* For iterating over attributes to serialize. */
   size_t size; /* Size of the string to allocate. */
   unsigned int var_count; /* To count the number of variables. */
   char * build_str; /* The string that is being built. */
   char * cur; /* Current location within the already allocated str. */
   char * tag_cur; /* Current position within tag string. */
   char * tag_end; /* end of current position within tag string. */

   size = 0;
   var_count = 0;


   /***** Decide on our looping mode. *****/
   if (tags == 0 || *tags == 0)
      tag_cur = 0;
   else
      tag_cur = (char *) tags;
   tag_end = tag_cur;
   var = 0;

   /***** Find the size of string needed for the attribute string. *****/
   while (var_iter(slp_attr, &tag_cur, &tag_end, &var))
   {
      /*** Skip bad tags? ***/
      if (var == 0)
         continue;

      /*** Skip old attributes. ***/
      if (find_delta == SLP_TRUE && var->modified == SLP_FALSE)
         continue;

      /*** Get size of tag ***/
      size += var->tag_len;

      /*** Count the size needed for the value list ***/
      if (var->type != SLP_KEYWORD)
      {
         value_t * value;

         /** Get size of data **/
         value = var->list;
         while (value)
         {
            size += value->escaped_len;
            value = value->next;
         }

         /** Count number of commas needed for multivalued attributes. **/
         SLP_ASSERT(var->list_size >= 0);
         size += (var->list_size - 1) * VAR_SEPARATOR_LEN;

         /** Count the semantics needed to store a multivalue list **/
         size += VAR_NON_KEYWORD_SEMANTIC_LEN;
      }
      else
         SLP_ASSERT(var->list == 0);

      /*** Count number of variables ***/
      var_count++;
   }

   /*** Count the number of characters between attributes. ***/
   if (var_count > 0)
      size += (var_count - 1) * VAR_SEPARATOR_LEN;


   /***** Return the size needed/used. *****/
   if (count != 0)
      *count = (int)(size + 1);

   /***** Create the string. *****/
   if (*out_buffer == 0)
   {
      /* We have to locally alloc the string. */
      build_str = (char *) malloc(size + 1);
      if (build_str == 0)
         return SLP_MEMORY_ALLOC_FAILED;
   }
   else
   {
      /* We write into a pre-alloc'd buffer. */
      /**** Check that out_buffer is big enough. ****/
      if (size + 1 > bufferlen)
         return SLP_BUFFER_OVERFLOW;
      build_str = *out_buffer;
   }
   build_str[0] = '\0';

   /***** Add values *****/

   /**** Decide on our looping mode. ****/
   if (tags == 0 || *tags == 0)
      tag_cur = 0;
   else
      tag_cur = (char *) tags;
   tag_end = tag_cur;
   var = 0;

   /**** Find the size of string needed for the attribute string. ****/
   cur = build_str;
   while (var_iter(slp_attr, &tag_cur, &tag_end, &var))
   {
      /*** Skip bad tags? ***/
      if (var == 0)
         continue;

      /*** Skip old attributes. ***/
      if (find_delta == SLP_TRUE && var->modified == SLP_FALSE)
         continue;

      if (var->type == SLP_KEYWORD)
      {
         /**** Handle keywords. ****/
         memcpy(cur, var->tag, var->tag_len);
         cur += var->tag_len;
      }
      else
      {
         /**** Handle everything else. ****/
         char * to_add;
         int to_add_len;
         value_t * value;

         /*** Add the prefix. ***/
         *cur = VAR_PREFIX;
         cur += VAR_PREFIX_LEN;

         /*** Add the tag. ***/
         memcpy(cur, var->tag, var->tag_len);
         cur += var->tag_len;

         /*** Add the infix. ***/
         *cur = VAR_INFIX;
         cur += VAR_INFIX_LEN;

         /*** Insert value (list) ***/
         value = var->list;
         SLP_ASSERT(value);
         while (value)
         {
            /* foreach member value of an attribute. */
            SLP_ASSERT(var->type != SLP_KEYWORD);
            switch (var->type)
            {
               case(SLP_BOOLEAN):
                  SLP_ASSERT(value->next == 0); /* Can't be a multivalued list. */
                  SLP_ASSERT(value->data.va_bool == SLP_TRUE
                        || value->data.va_bool == SLP_FALSE);

                  if (value->data.va_bool == SLP_TRUE)
                  {
                     to_add = BOOL_TRUE_STR;
                     to_add_len = BOOL_TRUE_STR_LEN;
                  }
                  else
                  {
                     to_add = BOOL_FALSE_STR;
                     to_add_len = BOOL_FALSE_STR_LEN;
                  }
                  memcpy(cur, to_add, to_add_len);
                  cur += to_add_len;
                  break;

               case(SLP_STRING):
                  cur = escape_into(cur, value->data.va_str,
                        (int)value->unescaped_len);
                  break;

               case(SLP_INTEGER):
                  sprintf(cur, "%d", value->data.va_int);
                  cur += value->escaped_len;
                  break;

               case(SLP_OPAQUE):
                  memcpy(cur, OPAQUE_PREFIX, OPAQUE_PREFIX_LEN);
                  cur += OPAQUE_PREFIX_LEN;
                  cur = escape_opaque_into(cur, value->data.va_str,
                        value->unescaped_len);
                  break;

               default:
                  printf("Unknown type (%s:%d).\n", __FILE__, __LINE__);
                  /* TODO Clean up memory leak: the output string */
                  return SLP_INTERNAL_SYSTEM_ERROR;
            }

            value = value->next;
            /*** Add separator (if necessary) ***/
            if (value != 0)
            {
               *cur = VAR_SEPARATOR;
               cur += VAR_SEPARATOR_LEN;
               *cur = 0;
            }
         }

         /*** Add the suffix. ***/
         *cur = VAR_SUFFIX;
         cur += VAR_SUFFIX_LEN;
         *cur = 0;
      }

      /*** Add separator. This is fixed for the last val outside the loop ***/
      /* if (var->next != 0) { */
      if ((unsigned) (cur - build_str) < size)
      {
         *cur = VAR_SEPARATOR;
         cur += VAR_SEPARATOR_LEN;
         *cur = 0;
      }

      /*** Reset the modified flag. ***/
      var->modified = SLP_FALSE;
   }


   /**** Shift back to erase the last comma. ****/
   /***** Append a null (This is actually done by strcpy, but its better to
    * be safe than sorry =) *****/
   *cur = '\0';

   SLP_ASSERT((unsigned) (cur - build_str) == size && size == strlen(build_str));

   *out_buffer = build_str;

   return SLP_OK;
}   



/* Stores an escaped value into an attribute. Determines type of attribute at 
 * the same time.
 *
 * tag must be null terminated. 
 * val must be of length len.
 * policy will only be respected where it can be (ints, strings, and opaques).
 *
 * the contents of tag are NOT verified. 
 * 
 * Returns: 
 *  SLP_PARAMETER_BAD - Syntax error in the value.
 *  SLP_MEMORY_ALLOC_FAILED
 */
static SLPError SLPAttrStore(struct xx_SLPAttributes * slp_attr, const char * tag,
      const char * val, size_t len, SLPInsertionPolicy policy)
{
   unsigned i; /* Index into val. */
   SLPBoolean is_str; /* Flag used for checking if given is string. */
   char * unescaped;
   size_t unescaped_len; /* Length of the unescaped text. */

   /***** Check opaque. *****/
   if (strncmp(val, OPAQUE_PREFIX, OPAQUE_PREFIX_LEN) == 0)
   {
      /*** Verify length (ie, that it is the multiple of the size of an
       * escaped character). ***/
      if (len % ESCAPED_LEN != 0)
         return SLP_PARAMETER_BAD;
      unescaped_len = (len / ESCAPED_LEN) - 1; /* -1 to drop the OPAQUE_PREFIX. */

      /*** Verify that every character has been escaped. ***/
      /* TODO */

      /***** Unescape the value. *****/
      unescaped = (char *) malloc(unescaped_len);
      if (unescaped == 0)
      {
         return SLP_MEMORY_ALLOC_FAILED; /* FIXME: Real error code. */
      }

      if (unescape_into(unescaped, (char *) (val + OPAQUE_PREFIX_LEN),
               len - OPAQUE_PREFIX_LEN, 0) != 0)
      {
         SLPError err;
         err = SLPAttrSet_opaque((SLPAttributes) slp_attr, tag, unescaped,
                     (len - OPAQUE_PREFIX_LEN) / 3, policy);
         free(unescaped);/* FIXME This should be put into the val, and free()'d in val_destroy(). */

         return err;
      }
      return SLP_PARAMETER_BAD; /* FIXME Verify. Is this really a bad parameter?*/
   }

   /***** Check boolean. *****/
   if ((BOOL_TRUE_STR_LEN == len) && (strncmp(val, BOOL_TRUE_STR, len) == 0))
      return SLPAttrSet_bool((SLPAttributes) slp_attr, tag, SLP_TRUE);
   if ((BOOL_FALSE_STR_LEN == len) && strncmp(val, BOOL_FALSE_STR, len) == 0)
      return SLPAttrSet_bool((SLPAttributes) slp_attr, tag, SLP_FALSE);


   /***** Check integer *****/
   if (*val == '-' || isdigit((int) * val))
   {
      /*** Verify. ***/
      SLPBoolean is_int = SLP_TRUE; /* Flag true if the attr is an int. */
      for (i = 1; i < len; i++)
      {
         /* We start at 1 since first char has already been checked. */
         if (!isdigit((int) val[i]))
         {
            is_int = SLP_FALSE;
            break;
         }
      }

      /*** Handle the int-ness. ***/
      if (is_int == SLP_TRUE)
      {
         char * end; /* To verify that the correct length was read. */
         SLPError err;
         err = SLPAttrSet_int((SLPAttributes) slp_attr, tag,
                     (int) strtol(val, &end, 10), policy);
         SLP_ASSERT(end == val + len);
         return err;
      }
   }

   /***** Check string. *****/
   is_str = SLP_TRUE;
   for (i = 0; i < len; i++)
   {
      if (IS_RESERVED(val[i]) && (val[i] != '\\'))
      {
         is_str = SLP_FALSE;
         break;
      }
   }
   if (is_str == SLP_TRUE)
   {
      unescaped_len = find_unescaped_size(val, len);
      unescaped = (char *) malloc(unescaped_len + 1); 
      if (unescape_into(unescaped, val, len, 0) != 0)
      {
         SLPError err;
         unescaped[unescaped_len] = '\0';
         err = SLPAttrSet_str((SLPAttributes) slp_attr, tag, unescaped, policy);
         free(unescaped); /* FIXME This should be put into the val, and free()'d in val_destroy(). */
         return err;
      }

      return SLP_PARAMETER_BAD;
   }


   /* We don't bother checking for a keyword attribute since it can't have a
    * value. 
    */

   return SLP_PARAMETER_BAD; /* Could not determine type. */
}



/* Converts an attribute string into an attr struct. 
 *
 * Note that the attribute string is trashed.
 *
 * Returns:
 *  SLP_PARAMETER_BAD -- If there is a parse error in the attribute string. 
 *  SLP_OK -- If everything went okay.
 */
static SLPError attr_destringify(struct xx_SLPAttributes * slp_attr,
      char const * str, SLPInsertionPolicy policy)
{
   char const *cur; /* Current index into str. */
   enum
   {
      /* Note: everything contained in []s in this enum is a production from
      * RFC 2608's grammar defining attribute lists. 
      */
      START_ATTR /* The start of an individual [attribute]. */,
      START_TAG /* The start of a [attr-tag]. */,
      VALUE /* The start of an [attr-val]. */,
      STOP_VALUE /* The end of an [attr-val]. */
   } state = START_ATTR; /* The current state of the parse. */
   char const *tag; /* A tag that has been parsed. (carries data across state changes)*/
   int tag_len = 0; /* length of the tag (in bytes) */

   (void)policy;

   SLP_ASSERT(str != 0);
   if (strlen(str) == 0)
      return SLP_OK;

   tag = 0;
   cur = str;
   /***** Pull apart str. *****/
   while (*cur)
   {
      char const *end; /* The end of a parse entity. */
      switch (state)
      {
         case(START_ATTR):
            /* At the beginning of an attribute. */
            if (*cur == VAR_PREFIX)
            {
               /* At the start of a non-keyword. */
               state = START_TAG;
               cur += VAR_PREFIX_LEN;
            }
            else
            {
               /* At the start of a keyword:
                * Gobble up the keyword and set it. 
                */
               end = find_tag_end(cur);

               if (end == 0)
               {
                  /* FIXME Ummm, I dunno. */
                  SLP_ASSERT(0);
               }
               /*** Check that the tag ends on a legal ending char. ***/
               if (*end == ',')
               {
                  /** Add the keyword. **/
                  SLPAttrSet_keyw_len((SLPAttributes) slp_attr, cur, end - cur);
                  cur = end + 1;
                  break;
               }
               else if (*end == '\0')
               {
                  SLPAttrSet_keyw_len((SLPAttributes) slp_attr, cur, end - cur);
                  return SLP_OK; /* FIXME Return success. */
                  break;
               }
               else
               {
                  return SLP_PARAMETER_BAD; /* FIXME Return error code. -- Illegal tag char */
               }
            }
            break;
         case(START_TAG):
            /* At the beginning of a tag, in brackets. */
            end = find_tag_end(cur);

            if (end == 0)
            {
               return SLP_PARAMETER_BAD; /* FIXME Err. code -- Illegal char. */
            }

            if (*end == '\0')
            {
               return SLP_PARAMETER_BAD; /* FIXME err: Premature end. */
            }

            /*** Check the the end character is valid. ***/
            if (*end == VAR_INFIX)
            {
               tag_len = (int) (end - cur); /* Note that end is on the character _after_ the last character of the tag (the =). */
               SLP_ASSERT(tag == 0);
               tag = cur; 
               cur = end + VAR_INFIX_LEN;
               state = VALUE;
            }
            else
            {
               /** ERROR! **/
               return SLP_PARAMETER_BAD; /* FIXME Err. code.-- Logic error. */
            }

            break;

         case(VALUE):
            /* At the beginning of the value portion. */
            SLP_ASSERT(tag != 0); /* We should not be able to get into this state: is the string is malformed? */
            {
               /*** Find the end of the entire value list. ***/
               int errval;
               int val_count;
               SLPType type;
               char const *start;
               size_t unescaped_len;

               start = cur;

               errval = find_value_list_end(start, &val_count, &type,
                     &unescaped_len, &cur);
               if (errval != 1)
                  return SLP_PARAMETER_BAD;

               if(unescaped_len)  /*Ignore an attribute that has no value*/
               {
                  errval = internal_store(slp_attr, tag, tag_len, start, cur,
                                 val_count, type, unescaped_len);
                  if (errval != 1)
                     return SLP_MEMORY_ALLOC_FAILED;
               }

               state = STOP_VALUE;
            }
            break;
         case(STOP_VALUE):
            /* At the end of a value. */
            /***** Check to see which state we should move into next.*****/
            /*** Done? ***/
            if (*cur == '\0')
               return SLP_OK;
            /*** Another value? (ie, we're in a value list) ***/
            //          else if (*cur == VAR_SEPARATOR) {
            //             cur += VAR_SEPARATOR_LEN;
            //             state = VALUE;
            //          }
            /*** End of the attribute? ***/
            else if (*cur == VAR_SUFFIX)
            {
               SLP_ASSERT(tag != 0);
               tag = 0;
               cur += VAR_SUFFIX_LEN;

               /*** Are we at string end? ***/
               if (*cur == '\0')
                  return SLP_OK;

               /*** Ensure that there is a seperator ***/
               if (*cur != VAR_SEPARATOR)
               {
                  return  SLP_PARAMETER_BAD; /* FIXME err -- unexpected character. */
               }

               cur += VAR_SEPARATOR_LEN;
               state = START_ATTR;
            }
            /*** Error. ***/
            else
            {
               return SLP_PARAMETER_BAD; /* FIXME err -- Illegal char at value end. */
            }
            break;
         default:
            printf("Unknown state %d\n", state);
      }
   }

   return SLP_OK;
}

//static void destringify(SLPAttributes slp_attr_h, const char * str)
//{
//   attr_destringify((struct xx_SLPAttributes *) slp_attr_h, (char *) str,
//         SLP_ADD);
//}


/* Adds the tags named in attrs to the receiver. Note that the new attributes 
 * _replace_ the old ones.
 *
 * Returns:
 *  SLP_OK -- Update occured as expected.
 *  SLP_MEMORY_ALLOC_FAILED -- Guess.
 *  SLP_PARAMETER_BAD -- Syntax error in the attribute string. Although 
 *    slp_attr_h is still valid, its contents may have arbitrarily changed. 
 */
SLPError SLPAttrFreshen(SLPAttributes slp_attr_h, const char * str)
{
   SLPError err;
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *)
         slp_attr_h;

   // char *mangle; /* A copy of the passed in string, since attr_destringify tends to chew data. */
   // 
   // mangle = strdup(str);
   // if (str == 0) {
   //    return SLP_MEMORY_ALLOC_FAILED;
   // }
   err = attr_destringify(slp_attr, str, SLP_ADD);
   // free(mangle);

   return err;
}

#ifdef ENABLE_PREDICATES
/* Processes a search string to make it compatible with an index created
 * from attribute strings
 *
 * Params:
 *  search_str -- (IN) the string we'll use to search
 *  search_str_len -- (IN) the length of the string
 *  processed_str -- (OUT) processed string
 *  processed_len -- (OUT) number of characters in processed string
 *
 * Returns:
 *  SLP_OK on success, error code on failure
 */
SLPError SLPAttributeSearchString(size_t search_str_len, const char * search_str, size_t *processed_len, char * *processed_str)
{
   char *x;
   char *dest = (char *)malloc(search_str_len + 1);
   *processed_len = 0;
   *processed_str = (char *)0;
   if (!dest)
      return SLP_MEMORY_ALLOC_FAILED;

   /* The only processing we do on attribute strings is to unescape them (see internal_store()) */
   x = unescape_into(dest, search_str, search_str_len, processed_len);
   if (!x)
   {
      /* The search string was malformed */
      free(dest);
      return SLP_PARSE_ERROR;
   }
   dest[*processed_len] = '\0';
   *processed_str = dest;
   return SLP_OK;
}
#endif /* ENABLE_PREDICATES */

/******************************************************************************
 *
 *                          SLP Control Functions
 *
 *****************************************************************************/

/* Register attributes. */
//SLPError SLPRegAttr( 
// SLPHandle slp_h, 
// const char* srvurl, 
// unsigned short lifetime, 
// const char* srvtype, 
// SLPAttributes attr_h, 
// SLPBoolean fresh, 
// SLPRegReport callback, 
// void* cookie 
//) {
// char *str;
// SLPError err;
// 
////  err = SLPAttrSerialize(attr_h, int *count, char **str, SLPBoolean find_delta); 
//
// if (str == 0) {
//    return SLP_INTERNAL_SYSTEM_ERROR;
// }
//
// err = SLPReg(slp_h, srvurl, lifetime, srvtype, str, fresh, callback, cookie);
//
// free(str);
//
// return err;
//}
//
//
//struct hop_attr
//{
//   SLPAttrObjCallback * cb;
//   void * cookie;
//};
//
//
//static SLPBoolean attr_callback(SLPHandle hslp, const char * attrlist,
//      SLPError errcode, void * cookie)
//{
//   struct hop_attr * hop = (struct hop_attr *) cookie;
//   SLPAttributes attr;
//   SLPBoolean result;
//
//   SLP_ASSERT(errcode == SLP_OK || errcode == SLP_LAST_CALL);
//
//   if (errcode == SLP_OK)
//   {
//      if (SLPAttrAlloc("en", 0, SLP_FALSE, &attr) != SLP_OK)
//      {
//         /* FIXME Ummm, should prolly tell application about the internal
//          * error.  
//          */
//         return SLP_FALSE;
//      }
//
//      destringify(attr, attrlist);
//      result = hop->cb(hslp, attr, errcode, hop->cookie);
//      SLP_ASSERT(result == SLP_TRUE || result == SLP_FALSE);
//      SLPAttrFree(attr);
//   }
//   else if (errcode == SLP_LAST_CALL)
//      result = hop->cb(hslp, 0, errcode, hop->cookie);
//   else
//      result = hop->cb(hslp, 0, errcode, hop->cookie);
//
//   return result;
//}
//
//
/* Find the attributes of a given service. */
//SLPError SLPFindAttrObj(
//    SLPHandle hslp, 
//    const char* srvurlorsrvtype, 
//    const char* scopelist, 
//    const char* attrids, 
//    SLPAttrObjCallback *callback, 
//    void* cookie
//) {
// struct hop_attr *hop;
// SLPError err;
// 
// hop = (struct hop_attr*)malloc(sizeof(struct hop_attr));
// hop->cb = callback;
// hop->cookie = cookie;
//
// err = SLPFindAttrs(hslp, srvurlorsrvtype, scopelist, attrids, attr_callback, hop);
//
// free(hop);
// 
// return err;
//}


/******************************************************************************
 *
 *                                Iterators
 *
 *****************************************************************************/

// /* An iterator to make for easy looping across the struct. */
//struct xx_SLPAttrIterator {
// int element_count; /* Number of elements. */
// char **tags; /* Array of tags. */
// int current; /* Current index into the attribute iterator. */
// struct xx_SLPAttributes *slp_attr;
//};
//
//
//
// /* Allocates a new iterator for the given attribute handle. */
//SLPError SLPAttrIteratorAlloc(SLPAttributes attr_h, SLPAttrIterator *iter_h) {
// struct xx_SLPAttrIterator *iter;
// struct xx_SLPAttributes *slp_attr = (struct xx_SLPAttributes *)attr_h;
// var_t *var;
// int i;
//
// SLP_ASSERT(slp_attr != 0);
//
// iter = (struct xx_SLPAttrIterator *)malloc(sizeof(struct xx_SLPAttrIterator)); /* free()'d in SLPAttrIteratorFree(). */
// if (iter == 0) {
//    return SLP_MEMORY_ALLOC_FAILED;
// }
//
// iter->element_count = (int)slp_attr->attr_count;
// iter->current = -1;
// iter->slp_attr = slp_attr;
//
// iter->tags = (char **)malloc(sizeof(char *) * iter->element_count);
// if (iter->tags == 0) {
//    free(iter);
//    return SLP_MEMORY_ALLOC_FAILED;
// }
// 
// var = slp_attr->attrs;
//
// for (i = 0; i < iter->element_count; i++, var = var->next) {
//    SLP_ASSERT(var != 0);
//    
//    iter->tags[i] = strdup(var->tag);
//    
//    /***** Check that strdup succeeded. *****/
//    if (iter->tags[i] == 0) {
//       /**** Unallocate structure. ****/
//       int up_to_i;
//       /*** Unallocate the tag list members. ***/
//       for (up_to_i = 0; up_to_i < i; up_to_i++) {
//          free(iter->tags[up_to_i]);
//       }
//       
//       /*** Unallocate the tag list ***/
//       free(iter->tags);
//       
//       return SLP_MEMORY_ALLOC_FAILED;
//    }
// }
//
// *iter_h = (SLPAttrIterator)iter;
// 
// return SLP_OK;
//}
//
//
// /* Dealloc's an iterator and the associated memory. 
// *
// * Everything free()'d here was alloc'd in SLPAttrIteratorAlloc(). 
// */
// static void SLPAttrIteratorFree(SLPAttrIterator iter_h) {
// struct xx_SLPAttrIterator *iter = (struct xx_SLPAttrIterator*)iter_h;
// int i;
//
// /***** Free the tag list. *****/
// for(i = 0; i < iter->element_count; i++) {
//    free(iter->tags[i]);
//    iter->tags[i] = 0;
// }
// 
// free(iter->tags);
// 
// free(iter); 
//}
//
//
// /* Gets the next tag name (and type). 
// *
// * Note: The value of tag _must_ be copied out before the next call to 
// *  SLPAttrIterNext(). In other words, DO NOT keep pointers to the tag string 
// *  after the next call to SLPAttrIterNext().
// *
// * Returns SLP_FALSE if there are no tags left to iterate over, or SLP_TRUE.
// */
//SLPBoolean SLPAttrIterNext(SLPAttrIterator iter_h, char const **tag, SLPType *type) {
// struct xx_SLPAttrIterator *iter = (struct xx_SLPAttrIterator*)iter_h;
// SLPError err;
//
// iter->current++;
// if (iter->current >= iter->element_count) {
//    *tag = 0;
//    return SLP_FALSE; /* FIXME Return Done. */
// }
// *tag = iter->tags[iter->current];
//    err = SLPAttrGetType(iter->slp_attr, *tag, type);
//
// if (err != SLP_OK) {
//    return SLP_FALSE; /* FIXME Ummm, try to get the next one. */
// }
// 
// return SLP_TRUE;
//}
//
//
//
//

/* An iterator to make for easy looping across the struct. */
struct xx_SLPAttrIterator
{
   struct xx_SLPAttributes * slp_attr;
   var_t * current;
   value_t * current_value;
};



/* Allocates a new attribute iterator for the given attribute handle. */
SLPError SLPAttrIteratorAlloc(SLPAttributes attr_h, SLPAttrIterator * iter_h)
{
   struct xx_SLPAttrIterator * iter;
   struct xx_SLPAttributes * slp_attr = (struct xx_SLPAttributes *) attr_h;

   SLP_ASSERT(slp_attr != 0);

   iter = (struct xx_SLPAttrIterator *)
         malloc(sizeof(struct xx_SLPAttrIterator)); /* free()'d in SLPAttrIteratorFree(). */
   if (iter == 0)
      return SLP_MEMORY_ALLOC_FAILED;

   iter->current = 0;
   iter->current_value = 0;
   iter->slp_attr = slp_attr;

   *iter_h = (SLPAttrIterator) iter;

   return SLP_OK;
}


/* Dealloc's an iterator and the associated memory. 
 *
 * Everything free()'d here was alloc'd in SLPAttrIteratorAlloc(). 
 */
void SLPAttrIteratorFree(SLPAttrIterator iter_h)
{
   struct xx_SLPAttrIterator * iter = (struct xx_SLPAttrIterator *) iter_h;

   free(iter);
}

/* Gets the next tag name (and type). 
 *
 * Note: The value of tag _must_ be copied out before the next call to 
 *    SLPAttrIterNext(). In other words, DO NOT keep pointers to the tag string 
 *    after the next call to SLPAttrIterNext().
 *
 * Returns SLP_FALSE if there are no tags left to iterate over, or SLP_TRUE.
 */
SLPBoolean SLPAttrIterNext(SLPAttrIterator iter_h, char const * *tag,
      SLPType * type)
{
   struct xx_SLPAttrIterator * iter = (struct xx_SLPAttrIterator *) iter_h;

   iter->current_value = 0;
   if (iter->current == 0)
      iter->current = iter->slp_attr->attrs;
   else
      iter->current = iter->current->next;

   if (iter->current == 0)
   {
      return SLP_FALSE; /* Done. */
   }

   *tag = iter->current->tag;
   *type = iter->current->type;

   return SLP_TRUE;
}

/* Gets the next value. 
 *
 * Returns SLP_FALSE if there are no values left to iterate over, or SLP_TRUE.
 */
SLPBoolean SLPAttrIterValueNext(SLPAttrIterator iter_h, SLPValue *value)
{
   struct xx_SLPAttrIterator * iter = (struct xx_SLPAttrIterator *) iter_h;

   SLP_ASSERT(value != 0);

   if (iter->current == 0)
      iter->current = iter->slp_attr->attrs;
   if (iter->current == 0)
      return SLP_FALSE; /* No tags */

   if (iter->current_value == 0)
      iter->current_value = iter->current->list;
   else
      iter->current_value = iter->current_value->next;

   if (iter->current_value == 0)
   {
      return SLP_FALSE; /* Done. */
   }

   value->len = iter->current_value->unescaped_len;
   switch (iter->current->type)
   {
   case SLP_BOOLEAN:
      value->data.va_bool = iter->current_value->data.va_bool;
      break;
   case SLP_INTEGER:
      value->data.va_int = iter->current_value->data.va_int;
      break;
   default:
      value->data.va_str = iter->current_value->data.va_str;
      break;
   }

   return SLP_TRUE;
}

/*=========================================================================*/
