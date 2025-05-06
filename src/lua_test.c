#include <errno.h>
#include <iconv.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>

#include <websocket.h>

static volatile int force_exit = 0;

static void sigint_handler(int sig) { force_exit = 1; }

char *loadFileToString(const char *filename);
char *to_utf8(const char *in_charset, const char *input, size_t in_len);
char *from_utf8(const char *out_charset, const char *input, size_t in_len);
char *ltrim(char *str);
char *rtrim(char *str);

//---------------------------------------------------------------------------

static char code_cache[MAX_PAYLOAD_SIZE] = {'a', 'b', '\n', 0};
static size_t code_cache_len = 3;
static int store_code = 0;

lua_State *L = NULL;

//---------------------------------------------------------------------------

unsigned char *message_callback(const unsigned char *message, size_t len) {
  unsigned char *result = NULL;
  char *msg = from_utf8("ISO-8859-1", (const char *)message, len);
  if (msg != NULL) {
    // char *msg = iso_8859_1;
    if (strncmp(ltrim(msg), "^^", 2) == 0) {
      // printf("command: %s\n", msg);
      switch (msg[2]) {
      case 's': { // store code
        store_code = 1;
        // fall through to clear cache
      }
      case 'c': { // clear
        code_cache_len = 0;
        break;
      }
      case 'p': {
        result =
            (unsigned char *)to_utf8("ISO-8859-1", code_cache, code_cache_len);
        break;
      }
      case 'w': {
        store_code = 0;
        break;
      }
      case 'z': { // reboot script
        break;
      }
      case 'r': { // reboot device
        break;
      }
      default: {
      }
      }
    } else {
      if (store_code == 1) {
        size_t len = strlen(msg);
        memcpy(&code_cache[code_cache_len], msg, len);
        code_cache_len = code_cache_len + len;
      } else {
        printf("%s\n", msg);
      }
      free(msg);
    }
  }
  return result;
}

//---------------------------------------------------------------------------

int main(int argc, char **argv) {
  if (argc <= 1) {
    fprintf(stderr, "\nNo arguments provided\n");
    return 0;
  }

  signal(SIGINT, sigint_handler);

  L = luaL_newstate();
  luaL_openlibs(L);

  struct lws_context *context = create_context();

  register_message_callback(message_callback);

  while (!force_exit) {
    lws_service(context,
                50); //  libwebsockets service function.  Timeout = 50ms
  }

  // const char *filename = argv[1];
  //
  // // Create a sample file for testing
  // char *code = loadFileToString(filename);
  //
  // // Here we load the string and use lua_pcall for run the code
  // if (luaL_loadstring(L, code) == LUA_OK) {
  //   if (lua_pcall(L, 0, 0, 0) == LUA_OK) {
  //     // If it was executed successfuly we
  //     // remove the code from the stack
  //     lua_pop(L, lua_gettop(L));
  //   }
  // }

  // Work with lua API

  close_context(context);
  lua_close(L);

  return 0;
}

//-------------------------------------------------------------

char *loadFileToString(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror("Error opening file");
    return NULL;
  }

  // Determine the size of the file
  fseek(file, 0, SEEK_END);
  long fileSize = ftell(file);
  fseek(file, 0, SEEK_SET); // Go back to the beginning

  // Allocate memory for the string (+1 for the null terminator)
  char *buffer = (char *)malloc(fileSize + 1);
  if (buffer == NULL) {
    fclose(file);
    perror("Error allocating memory");
    return NULL;
  }

  // Read the entire file into the buffer
  size_t bytesRead = fread(buffer, 1, fileSize, file);
  if (bytesRead != fileSize) {
    fclose(file);
    free(buffer);
    perror("Error reading file");
    return NULL;
  }

  // Null-terminate the string
  buffer[fileSize] = '\0';

  fclose(file);
  return buffer;
}

/*
 * Function to convert a char* string to UTF-8.
 *
 * in_charset:  The input character set (e.g., "ISO-8859-1", "UTF-16LE").
 * input:      The input char* string.
 * in_len:     The length of the input string.
 *
 * Returns: A newly allocated char* containing the UTF-8 representation of the
 * input string. The caller is responsible for freeing the allocated memory.
 * Returns NULL on error.
 */
char *to_utf8(const char *in_charset, const char *input, size_t in_len) {
  char *output = NULL;
  size_t out_len = 0;
  size_t converted_len = 0;
  iconv_t cd = (iconv_t)-1;

  if (input == NULL || in_len == 0) {
    return NULL; // Handle empty input
  }

  // Open the conversion descriptor.
  cd = iconv_open("UTF-8", in_charset);
  if (cd == (iconv_t)-1) {
    perror("iconv_open failed");
    return NULL;
  }

  // Determine the output buffer size.  A rough estimate is 4 bytes per input
  // char.
  out_len = in_len * 4;
  output = (char *)malloc(out_len + 1); // +1 for null terminator
  if (output == NULL) {
    perror("malloc failed");
    iconv_close(cd);
    return NULL;
  }
  memset(output, 0, out_len + 1);

  char *out_ptr = output;
  char *in_ptr = (char *)input; // Cast const away, as iconv needs non-const

  // Perform the conversion.
  converted_len = iconv(cd, &in_ptr, &in_len, &out_ptr, &out_len);
  if (converted_len == (size_t)-1) {
    perror("iconv failed");
    free(output);
    iconv_close(cd);
    return NULL;
  }
  output[strlen(output)] = '\0'; // Ensure null termination

  iconv_close(cd);
  return output;
}

/*
 * Function to convert a UTF-8 char* string to another encoding.
 *
 * out_charset: The output character set (e.g., "ISO-8859-1", "UTF-16LE").
 * input:      The input UTF-8 char* string.
 * in_len:     The length of the input string.
 *
 * Returns: A newly allocated char* containing the converted string.
 * The caller is responsible for freeing the allocated memory.
 * Returns NULL on error.
 */
char *from_utf8(const char *out_charset, const char *input, size_t in_len) {
  char *output = NULL;
  size_t out_len = 0;
  size_t converted_len = 0;
  iconv_t cd = (iconv_t)-1;

  if (input == NULL || in_len == 0) {
    return NULL; // Handle empty input
  }

  // Open the conversion descriptor.
  cd = iconv_open(out_charset, "UTF-8");
  if (cd == (iconv_t)-1) {
    perror("iconv_open failed");
    return NULL;
  }

  // Determine the output buffer size.  A rough estimate is the input length.
  out_len = in_len;
  output = (char *)malloc(out_len + 1); // +1 for null terminator.
  if (output == NULL) {
    perror("malloc failed");
    iconv_close(cd);
    return NULL;
  }
  memset(output, 0, out_len + 1);

  char *out_ptr = output;
  char *in_ptr = (char *)input;

  // Perform the conversion.
  converted_len = iconv(cd, &in_ptr, &in_len, &out_ptr, &out_len);
  if (converted_len == (size_t)-1) {
    perror("iconv failed");
    free(output);
    iconv_close(cd);
    return NULL;
  }
  output[strlen(output)] = '\0'; // Ensure null termination

  iconv_close(cd);
  return output;
}

/*
 * Function to strip leading whitespace from a char* string.
 *
 * str: The input char* string to be modified.
 *
 * Returns: A pointer to the first non-whitespace character in the string.
 * The original string is modified in place.
 */
char *ltrim(char *str) {
  if (str == NULL || *str == '\0') {
    return str; // Handle null or empty string
  }

  size_t n = strspn(
      str, " \t\n\r"); // Use strspn to find the length of initial whitespace
  if (n > 0) {
    size_t len = strlen(str);
    memmove(str, str + n,
            len - n + 1); // Use memmove to shift the non-whitespace part
  }
  return str;
}

/*
 * Function to strip trailing whitespace from a char* string.
 *
 * str: The input char* string to be modified.
 *
 * Returns: The modified string (the original string, modified in place).
 */
char *rtrim(char *str) {
  if (str == NULL || *str == '\0') {
    return str; // Handle null or empty string
  }

  size_t len = strlen(str);
  size_t i = len;

  // Find the last non-whitespace character
  while (i > 0 && strchr(" \t\n\r", str[i - 1]) != NULL) {
    i--;
  }

  // Truncate the string
  str[i] = '\0';
  return str;
}
