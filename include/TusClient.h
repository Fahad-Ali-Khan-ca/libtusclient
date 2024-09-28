#ifndef INCLUDE_TUSCLIENT_H_
#define INCLUDE_TUSCLIENT_H_
/*
 * Copyright (c) 2024 Matteo Cadoni
 * This file is part of libtusclient, licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */
#include <string>
#include <memory>
#include <filesystem>
#include <boost/uuid/uuid.hpp>
#include <functional>
#include <atomic>
#include <map>


#include "libtusclient.h"
#include "TusStatus.h"
#include "http/IHttpClient.h"




using std::string;
using std::unique_ptr;
using std::filesystem::path;



/**
 * @brief The TusClient class represents a client for uploading files using the TUS protocol.
 * 
 * This class provides methods for uploading, canceling, resuming, stopping, and retrying file uploads.
 * It also provides methods for retrieving the upload progress and status.
 */
namespace TUS{
    class TUSFile;
    template<typename T>
    class IRepository;
    class TUSChunk;
    /*
    * @brief The ITusClient class represents an interface for a client for uploading files using the TUS protocol.
    */
    class LIBTUSAPI_EXPORT ITusClient
    {
    public:
        virtual void upload() = 0;
        virtual void cancel() = 0;
        virtual void resume() = 0;
        virtual void stop() = 0;
        virtual void pause() = 0;
        
        virtual float progress() = 0;
        virtual TusStatus status() = 0;
        virtual void retry() = 0;
        // Getters
        virtual path getFilePath() const = 0;
        virtual string getUrl() const = 0;

        

    };
    /**
     * @brief The TusClient class represents a client for uploading files using the TUS protocol.
     */
    class LIBTUSAPI_EXPORT TusClient: public ITusClient
    {
    private:
        using OnSuccessCallback = std::function<void(std::string,std::string)>;
        using OnErrorCallback = std::function<void(std::string,std::string)>;
        string m_url; 
        path m_filePath;
        std::atomic<TusStatus> m_status;
        path m_tempDir;
        
        
        int m_chunkSize;
        const string CHUNK_FILE_NAME_PREFIX = "_chunk_";
        const string CHUNK_FILE_EXTENSION = ".bin";
        int m_chunkNumber=0;
        int m_uploadedChunks=0;
        string m_tusLocation;
        int m_uploadOffset=0;
        bool m_nextChunk=false;
        size_t m_uploadLength=0;

        std::atomic<float> m_progress{0};  
        std::unique_ptr<Http::IHttpClient> m_httpClient;
        std::shared_ptr<TUSFile> m_tusFile;
        std::unique_ptr<IRepository<TUSFile>> m_cacheManager;

        std::vector<TUSChunk> m_chunks;

        std::string m_appName;

        /**
         * @brief Loads the chunks from the file.
         */
        void loadChunks();

        void getUploadInfo();

        void createTusFile();
        

        void wait(std::chrono::milliseconds ms, std::function<bool()> condition,std::string message);
        /**
         * @brief Divides the file into chunks.
         * 
         * @param path The path of the file to divide.
         * @return The number of chunks the file was divided into.
         */

        int divideFileInChunks(path filePath, boost::uuids::uuid uuid);

        /**
         * @brief Removes the chunk files, this when a chunk is uploaded successfully.
         * 
         * @param path The path of the file to remove the chunks from.
         */
        bool removeChunkFiles(path filePath);

        string getChunkFilename(int chunkNumber);

        boost::uuids::uuid m_uuid;

        string getUUIDString();
        path getTUSTempDir();

        void uploadChunks();

        void uploadChunk(int chunkNumber);

        
        void initialize();
        
    public:
        TusClient(string appName,string url, path filePath,int chunkSize);
        TusClient(string appName,string url, path filePath);
        ~TusClient();
        /**
         * @brief Uploads the file to the server using the TUS protocol.
         */
        void upload() override;
        /**
         * @brief Cancels the upload.
         */
        void cancel() override;
        /**
         * @brief Resumes the upload.
         */
        void resume() override;
        /**
         * @brief Stops the upload.
         */
        void stop() override;
        /**
         * @brief Returns the progress of the upload.
         * 
         * @return The progress of the upload as a percentage.
         */
        float progress() override;
        /**
         * @brief Returns the status of the upload.
         * 
         * @return The status of the upload.
         */
        TusStatus status() override;
        /**
         * @brief Retries the upload.
         */
        void retry() override;

        /**
         * @brief Pauses the upload.
         */
        void pause() override;

        /**
         * @brief Returns the server information for the TUS server.
         * this function can be used to understand which extensions are supported by the server.
         * @return The server information.
         */
        
        std::map<string,string> getTusServerInformation();

        path getFilePath() const override;
        string getUrl() const override;

    };
} // namespace TUS
#endif // INCLUDE_TUSCLIENT_H_