namespace os{
    u32 getFileFullName(char *filePath, char *buff) {
        char *fullpath = realpath(filePath, buff);
        if(fullpath == nullptr){return 0;};
        return strlen(fullpath);
    };

    void setPrintColorToWhite() {printf("\033[97m");};
    void printErrorInRed() {printf("\033[31mERROR\033[97m");};
    void printWarningInYellow(){printf("\033[1;33mWARNING\033[97m");}
}