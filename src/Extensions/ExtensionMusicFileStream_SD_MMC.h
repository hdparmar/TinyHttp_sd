#pragma once

#include <SD_MMC.h>
#include "Extensions/Extension.h"
#include "Server/HttpServer.h"
#include "Extensions/ExtensionStreamBasic.h"
#include "Server/HttpStreamedMultiOutput.h"

namespace tinyhttp {

/**
 * @brief Extension which supports the Streaming of music files from a SD drive. 
 * 
 */
class ExtensionMusicFileStream_SD_MMC : public Extension {
    public:
        ExtensionMusicFileStream_SD_MMC(const char*url="/music", const char* startDir="/", const char* mime="audio/mpeg", const char* extension="mp3", int bufferSize=512, int delay=10, int8_t cmdPin=15, int8_t clkPin=14, int8_t d0Pin=2){
            HttpLogger.log(Info,"ExtensionMusicFileStream_SD_MMC %s", url);
            this->url = url;
            this->file_extension = extension;
            this->start_dir = startDir;
            this->buffer_size = bufferSize;
            this->buffer = new uint8_t[bufferSize];
            HttpStreamedMultiOutput *out = new HttpStreamedMultiOutput(mime, nullptr, nullptr, 0);
            this->streaming = new ExtensionStreamBasic(url,  *out, T_GET);
            this->delay_ms = delay;
            this->cmd_pin = cmdPin;
            this->clk_pin = clkPin;
            this->d0_pin = d0Pin;

        }

        ~ExtensionMusicFileStream_SD_MMC(){
            HttpLogger.log(Info,"~ExtensionMusicFileStream_SD_MMC");
            delete[] buffer;
            delete this->streaming;
        }

        virtual void open(HttpServer *server) {
            HttpLogger.log(Info,"ExtensionMusicFileStream_SD_MMC %s", "open");
            setupSD();
            // setup handler
            streaming->open(server);
            // setup first music file
            HttpLogger.log(Info, "Opening directory: %s", start_dir);
            directory = SD_MMC.open(start_dir);
            if (!directory) {
                HttpLogger.log(Error, "Failed to open directory: %s", start_dir);
            return;
            }
            
            // find first music file - so that we are already ready to stream when a request arives
            File file = getMusicFile();
            if (file) {
                HttpLogger.log(Info, "Initial music file found: %s", file.name());
            } else {
                HttpLogger.log(Warning, "No initial music file found");
            }       
        }

    protected:
        ExtensionStreamBasic *streaming;
        const char *file_extension;
        const char *start_dir;
        const char *url;
        File directory;
        File current_file;
        File empty;
        uint8_t* buffer;
        int buffer_size;
        int loop_limit = 10;
        int loop_count;
        int8_t cmd_pin;
        int8_t clk_pin;
        int8_t d0_pin;
        int delay_ms;
        bool is_open = false;

        // incremental pushing of the next buffer size to the open clients using chunked HttpStreamedMultiOutput
        virtual void doLoop() override {
            HttpLogger.log(Debug, "doLoop called");
            if (streaming->isOpen()) {
                HttpLogger.log(Debug, "Streaming is open");
                File file = getMusicFile();
                if (file) {
                    HttpLogger.log(Debug, "Got music file: %s", file.name());
                    int len = file.read(buffer, buffer_size);
                    HttpLogger.log(Debug, "Read %d bytes from file %s", len, file.name());
                    if (len > 0) {
                        streaming->write(buffer, len);
                        delay(delay_ms);
                        //HttpLogger.log(Debug, "Wrote %d bytes to stream", written);
                    } else {
                        HttpLogger.log(Warning, "End of file reached or read error for %s", file.name());
                        file.close();
                    }
                } else {
                HttpLogger.log(Warning, "Failed to get a valid music file");
                }
            } 
            else {
                HttpLogger.log(Debug, "No active streaming clients");
            }
        }

        void setupSD() {
            if (!is_open) {
                SD_MMC.setPins(clk_pin, cmd_pin, d0_pin);
                if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)){
                    HttpLogger.log(Error, "Card Mount Failed!");
                }
                is_open = true;
            }
        }

        // provides the current file if it is not finished yet otherwise we move to the 
        // next music file or restart at the start directory when we reach the end
        File &getMusicFile() {
            HttpLogger.log(Debug, "getMusicFile called");
            HttpLogger.log(Debug,"ExtensionMusicFileStream_SD_MMC::getMusicFile %s",file_extension);
            if (current_file.available()>0){
                loop_count = 0;
                HttpLogger.log(Debug,"ExtensionMusicFileStream_SD_MMC::getMusicFile", current_file.name());
                return current_file;
            }
            current_file.close();
            int nextFileCount = 0;
            while(true){
                // prevent an endless loop
                if (loop_count > loop_limit){
                    return empty;
                }

                // check file extension
                current_file = directory.openNextFile();
                HttpLogger.log(Info,"processing", current_file.name());
                if (current_file){
                    Str name_str = Str(current_file.name());
                    if (name_str.endsWith(file_extension) && !name_str.contains("/.")){
                        HttpLogger.log(Info,"ExtensionMusicFileStream_SD_MMC::getMusicFile %s", current_file.name());
                        loop_count = 0;
                        nextFileCount = 0;
                        break;
                    } 
                    HttpLogger.log(Warning,"ExtensionMusicFileStream_SD_MMC::getMusicFile %s - not relevant", current_file.name());
                } else {
                    nextFileCount++;
                }

                // we restart when we did not find any vaild file in the last 20 entrie
                if (nextFileCount>20){
                    HttpLogger.log(Warning,"ExtensionMusicFileStream_SD_MMC::getMusicFile %s", "restart");
                    // no file -> restart from the beginning
                    directory.rewindDirectory();
                    loop_count++;
                }
            }    

            return current_file;
        }

};

}

