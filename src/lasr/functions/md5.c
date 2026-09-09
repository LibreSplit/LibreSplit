#include "lasr/functions/md5.h"
#include "logging.h"
#include "lua.h"
#include <openssl/evp.h>
#include <stdio.h>

const unsigned int MD5_FILE_READ_BUFFER_SIZE = 4096;

/**
 * Takes a path, reads the file and calculates an MD5 hash of it
 *
 * @param L The lua stack
 */
int md5sum(lua_State* L)
{
    if (lua_gettop(L) != 1) {
        // Call without arguments
        printf("[md5sum] This function requires the path to a file to calculate the MD5 checksum");
        lua_pushnil(L);
        return 1;
    }
    if (!lua_isstring(L, 1)) {
        // Called with a non-string argument
        printf("[md5sum] The argument must be a string");
        lua_pushnil(L);
        return 1;
    }
    const char* file_path = lua_tostring(L, 1);
    FILE* file_ptr = fopen(file_path, "r");
    if (file_ptr == NULL) {
        printf("[md5sum] Error while opening the file for MD5 hashing");
        lua_pushnil(L);
        return 1;
    }
    EVP_MD_CTX* md5_context = EVP_MD_CTX_new();
    if (!md5_context) {
        LOG_ERR("[md5sum] Cannot create EVP Digest Context");
        fclose(file_ptr);
        lua_pushnil(L);
        return 1;
    }
    if (EVP_DigestInit_ex(md5_context, EVP_md5(), NULL) != 1) {
        LOG_ERR("[md5sum] Cannot initialize EVP Digest Context");
        EVP_MD_CTX_free(md5_context);
        fclose(file_ptr);
        lua_pushnil(L);
        return 1;
    }
    char buffer[MD5_FILE_READ_BUFFER_SIZE];
    ssize_t bytes_read;
    do {
        bytes_read = fread(buffer, 1, MD5_FILE_READ_BUFFER_SIZE, file_ptr);
        // NOTE: [Penaz] [2026-06-05] Consider using ferror and feof to
        // ^ check for read errors and EOF errors
        if (EVP_DigestUpdate(md5_context, buffer, bytes_read) != 1) {
            LOG_ERR("[md5sum] Failed updating the EVP Digest Context");
            EVP_MD_CTX_free(md5_context);
            fclose(file_ptr);
            lua_pushnil(L);
            return 1;
        }
    } while (bytes_read > 0);
    unsigned int hash_length;
    unsigned char md5_out[EVP_MAX_MD_SIZE];
    if (EVP_DigestFinal_ex(md5_context, md5_out, &hash_length) != 1) {
        LOG_ERR("[md5sum] Failed finalizing the EVP Digest Context");
        EVP_MD_CTX_free(md5_context);
        fclose(file_ptr);
        lua_pushnil(L);
        return 1;
    }
    EVP_MD_CTX_free(md5_context);
    fclose(file_ptr);
    int pos = 0;
    char output[33];
    for (unsigned int i = 0; i < hash_length; i++) {
        pos += snprintf(output + pos, sizeof(output) - pos, "%02x", md5_out[i]);
    }
    lua_pushstring(L, output);
    return 1;
}
