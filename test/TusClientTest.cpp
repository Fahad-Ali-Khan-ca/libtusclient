/*
 * Copyright (c) 2024 Matteo Cadoni
 * This file is part of libtusclient, licensed under the MIT License.
 * See the LICENSE file in the project root for more information.
 */
#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <random>
#include <libzippp/libzippp.h>
#include <filesystem>
#include <thread>
#include <chrono>

#include "TusClient.h"
 /**
  * @brief These tests are integration tests that test the TusClient class
  */
namespace TUS::Test
{
	class TusClientTest : public ::testing::Test
	{
	protected:
		std::filesystem::path generateTestFile(int size = 10);
		std::filesystem::path generateSimpleFile();

		void SetUp() override
		{
			// Set up code here.
#ifdef WIN32
			//Windows has some problems with file generation, it crashes when it uploads the generated file, with correct files it works
			//TODO: investigate on windows and fix the problem, library should work on windows with no particular issues
			GTEST_SKIP();
#endif
		}

		void TearDown() override
		{
			if (std::filesystem::exists("test.txt"))
				std::filesystem::remove("test.txt");
			if (std::filesystem::exists("test.zip"))
				std::filesystem::remove("test.zip");
			// remove all .dat
			for (int i = 0; i < 1000; i++)
			{
				if (std::filesystem::exists(std::to_string(i) + ".dat"))
					std::filesystem::remove(std::to_string(i) + ".dat");
			}
		}
		Logging::LogLevel logLevel = Logging::LogLevel::_DEBUG_;
	};

    void waitUpload(const TusClient &client, float perc) {
        if (perc > 100) {
            perc = 100;
        } else if (perc <= 0) {
            perc = 1;
        }

        auto start = std::chrono::steady_clock::now(); 
        const std::chrono::seconds timeout(30); //30s of timeout

        while (client.progress() <= perc) {
            
            auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(
                100));
        }
    }

    std::filesystem::path TusClientTest::generateTestFile(int size)
	{
		// generate random .dat files 10MB each
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 255);
		std::vector<std::thread> threads;
		for (int i = 0; i < size; i++)
		{
			threads.emplace_back([&, i]()
				{
					std::string data;
					for (int j = 0; j < 1024 * 1024; j++) {
						data.push_back(dis(gen));
					}
					std::ofstream datFile(std::filesystem::current_path() / (std::to_string(i) + ".dat"), std::ios::binary);
					datFile.write(data.c_str(), data.size());
					datFile.close(); });
		}
		for (auto& thread : threads)
		{
			thread.join();
		}
		// Zip the files
		libzippp::ZipArchive zipArchive((std::filesystem::current_path() / "test.zip").string());
		zipArchive.open(libzippp::ZipArchive::New);

		for (int i = 0; i < size; i++)
		{
			zipArchive.addFile(std::to_string(i), (std::filesystem::current_path() / (std::to_string(i) + ".dat")).string());

		}
		zipArchive.close();
		// remove the .dat files
		for (int i = 0; i < size; i++)
		{
			std::filesystem::remove(std::filesystem::current_path() / (std::to_string(i) + ".dat"));
		}
		return std::filesystem::current_path() / "test.zip";

	}

	std::filesystem::path TusClientTest::generateSimpleFile()
	{
		std::ofstream testFile("test.txt");
		testFile << "Hello World";
		testFile.close();
		return "test.txt";
	}

	TEST_F(TusClientTest, clientCreationTest)
	{
		// create text file
		TUS::TusClient client("testapp", "http://localhost:8080/files", generateSimpleFile());

		EXPECT_EQ(client.getUrl(), "http://localhost:8080/files");
		EXPECT_EQ(client.getFilePath(), "test.txt");
		EXPECT_EQ(client.status(), TUS::TusStatus::READY);
		std::filesystem::remove("test.txt");
	}

	TEST_F(TusClientTest, uploadTest)
	{
		std::filesystem::path testFilePath = generateTestFile(10);
		std::cout << "Test file path: " << testFilePath << std::endl;
		TUS::TusClient client("testapp", "http://localhost:8080", testFilePath,logLevel);

		client.upload();

		EXPECT_EQ(client.status(), TUS::TusStatus::FINISHED);
	}

	TEST_F(TusClientTest, uploadBigTest)
	{

		std::filesystem::path testFilePath = generateTestFile(100);
		std::cout << "Test file path: " << testFilePath << std::endl;
		TUS::TusClient client("testapp", "http://localhost:8080", testFilePath, logLevel);

		client.upload();

		EXPECT_EQ(client.status(), TUS::TusStatus::FINISHED);
	}

	TEST_F(TusClientTest, pauseTest)
	{
		
		std::filesystem::path testFilePath = generateTestFile(10);
		std::cout << "Test file path: " << testFilePath << std::endl;
		TUS::TusClient client("testapp", "http://localhost:8080", testFilePath, logLevel);
		client.setRequestTimeout(std::chrono::milliseconds(100));

		std::thread uploadThread([&]()
			{ client.upload(); });
		waitUpload(client, 10);
		client.pause();
        uploadThread.join();
		EXPECT_EQ(client.status(), TUS::TusStatus::PAUSED);
		
	}

	TEST_F(TusClientTest, pauseResumeTest)
	{
		std::filesystem::path testFilePath = generateTestFile(10);
		std::cout << "Test file path: " << testFilePath << std::endl;
		TUS::TusClient client("testapp", "http://localhost:8080", testFilePath, logLevel);
		client.setRequestTimeout(std::chrono::milliseconds(10));
		std::thread uploadThread([&]()
			{ client.upload(); });
		waitUpload(client, 10);

		client.pause();
		std::cout << "Pause" << std::endl;


		uploadThread.join();
        EXPECT_EQ(client.status(), TUS::TusStatus::PAUSED);

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		std::cout << "Resuming" << std::endl;
		float progress = client.progress();
		std::thread resumeThread([&]()
			{ client.resume(); });

		waitUpload(client, progress);

		resumeThread.join();
	}

	TEST_F(TusClientTest, getServerInformationTest)
	{
		TUS::TusClient client("testapp", "http://localhost:8080/files", generateSimpleFile());
		std::map<std::string, std::string> serverInformation = client.getTusServerInformation();
		EXPECT_EQ(serverInformation["Tus-Resumable"], "1.0.0");
		EXPECT_EQ(serverInformation["Tus-Version"], "1.0.0");
		EXPECT_EQ(serverInformation["Tus-Extension"], "creation,creation-with-upload,termination,concatenation,creation-defer-length");
	}

	TEST_F(TusClientTest, cancelUpload)
	{
		std::filesystem::path testFilePath = generateTestFile(10);
		std::cout << "Test file path: " << testFilePath << std::endl;
		TUS::TusClient client("testapp", "http://localhost:8080", testFilePath, logLevel);
		client.setRequestTimeout(std::chrono::milliseconds(10));

		std::thread uploadThread([&]()
			{ client.upload(); });
		std::this_thread::sleep_for(std::chrono::seconds(1));

		client.cancel();
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait for the thread to finish

		EXPECT_EQ(client.status(), TUS::TusStatus::CANCELED);
		uploadThread.join();
	}

	TEST_F(TusClientTest, retryUpload)
	{
		std::filesystem::path testFilePath = generateTestFile(10);
		std::cout << "Test file path: " << testFilePath << std::endl;
		TUS::TusClient client("testapp", "http://localhost:8080", testFilePath, logLevel);
		client.setRequestTimeout(std::chrono::milliseconds(10));

		std::thread uploadThread([&]() { client.upload(); });
		waitUpload(client, 10);

		client.cancel();
		uploadThread.join();

		EXPECT_EQ(client.status(), TUS::TusStatus::CANCELED);
		client.retry();

		EXPECT_EQ(client.status(), TUS::TusStatus::FINISHED);
	}

}