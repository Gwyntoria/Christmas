/*
 * ConfigParser.h:
 *
 ***********************************************************************
 * by Jessica Mao
 * Lotogram Inc,. 2020/04/17
 *
 ***********************************************************************
 */


#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#define PUSH_CONFIG_FILE_PATH "/root/push.conf"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief delete a character from a string
 */
void DeleteChar(char str[], char target);

/**
 * @brief Get the Ini Key String object
 * 
 * @param title title of configuration section
 * @param key Identification of the value to be read in this configuration section
 * @param filename the path of the config file
 * @return char* If the value to be queried is found, the correct result will be returned; 
 *               Otherwise, NULL will be returned
 */
char *GetConfigKeyValue(const char *title, const char *key, const char *filename);

/**
 * @brief Get the Ini Key String object
 * 
 * @param title title of configuration section
 * @param key 这组数据中要读出的值的标识
 * @param filename 要读取的文件路径
 * @return int 找到需要查的值则返回正确结果,否则返回NULL
 */
int GetIniKeyInt(char *title ,char *key, char *filename);

/**
 * @brief Modify the value of the specified key
 * 
 * @param title 配置文件中一组数据的标识
 * @param key 这组数据中要读出的值的标识
 * @param val 更改后的值
 * @param filename 要读取的文件路径
 * @return int 0: success, -1: failure
 */
int PutConfigKeyValue(const char *title, const char *key, const char *val, const char *filename);

/**
 * @brief 
 * 
 * @param title 配置文件中一组数据的标识
 * @param key 这组数据中要读出的值的标识
 * @param val 更改后的值
 * @param filename 要读取的文件路径
 * @return int 0: success, -1: failure
 */
int PutIniKeyInt(char *title, char *key, int val, char *filename);

#ifdef __cplusplus
}
#endif


#endif
