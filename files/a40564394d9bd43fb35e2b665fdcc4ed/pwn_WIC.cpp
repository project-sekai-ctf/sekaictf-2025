#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <Wincodecsdk.h>
#include "base64.h"

using namespace std;

#define MaxSize 0x5fff
#define MAX_DECODED_SIZE  (1024 * 1024)       // 1 MB decoded buffer
#define MAX_ENCODED_SIZE  (((MAX_DECODED_SIZE + 2) / 3) * 4)  // 1.33 MB encoded buffer

void* HeapBuf[10];
HANDLE hHeap = GetProcessHeap();
HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

void CleanupTempFiles() {
    DeleteFileA("C:\\CTF\\test.jpg");
    DeleteFileA("C:\\CTF\\test2.jpg");
}

BOOL read_int(int* outValue) {
    char buf[32] = { 0 };  // Enough for any reasonable int string
    DWORD bytesRead = 0;

    if (!ReadFile(hStdin, buf, sizeof(buf) - 1, &bytesRead, NULL) || bytesRead == 0) {
        fprintf(stderr, "Failed to read input: error %lu\n", GetLastError());
        return FALSE;
    }

    // Null-terminate in case input doesn't end with \0
    buf[bytesRead] = '\0';

    *outValue = atoi(buf);
    return TRUE;
}

size_t read_input(unsigned char* buffer, size_t maxSize) {
    size_t totalRead = 0;
    int ch;

    while (totalRead < maxSize) {
        ch = fgetc(stdin);

        if (ch == EOF || ch == '\n')
            break;

        buffer[totalRead++] = (unsigned char)ch;
    }

    return totalRead;
}

void PrintMenu()
{
    printf("--- JPEG CTF Challenge Menu ---\n");
    printf("1. Create Heap\n");
    printf("2. Free Heap\n");
    printf("3. Process Jpeg\n");
    printf("4. Exit\n");
    printf("-------------------------------\n");
    printf("Choose option: ");
    fflush(stdout);

}

void CreateHeap()
{
    int size, idx;
    printf("Index: ");
    read_int(&idx);
    printf("Size: ");
    read_int(&size);

    if (idx < 0 || idx >= 10 || size <= 0 || size > 0x5fff)
    {
        printf("Invalid");
        return;
    }

    HeapBuf[idx] = HeapAlloc(hHeap, 0, size);

    if (HeapBuf[idx] == NULL)
    {
        printf("Invalid");
        return;
    }
    printf("Data: ");
    read_input((unsigned char*)HeapBuf[idx], size);
}

void DeleteHeap()
{
    int idx;
    printf("Index: ");
    read_int(&idx);
    if (idx < 0 || idx >= 10 || HeapBuf[idx] == NULL) {
        printf("Invalid index or already freed.\n");
        return;
    }
    HeapFree(GetProcessHeap(), 0, HeapBuf[idx]);
    HeapBuf[idx] = NULL;
    printf("Heap Free'd\n");
}

void ReadJpeg()
{
    int readSize, dec_len = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD written = 0;
    printf("Enter base64 encoded JPG size: ");
    read_int(&readSize);
    if (readSize <= 0 || readSize >= MAX_ENCODED_SIZE) {
        printf("Invalid size.\n");
        return;
    }

    // Allocate buffer with VirtualAlloc
    unsigned char* encodedBuffer = (unsigned char*)VirtualAlloc(NULL, MAX_ENCODED_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    unsigned char* decodedBuffer = (unsigned char*)VirtualAlloc(NULL, MAX_DECODED_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!encodedBuffer || !decodedBuffer) {
        printf("VirtualAlloc failed");
        return;
    }

    printf("base64 encoded JPG data: ");
    int enc_len = read_input(encodedBuffer, readSize);
    if (enc_len != readSize) {
        printf("Failed to read the full encoded input. Got %d bytes, expected %d.\n", enc_len, readSize);
        goto fail;
    }

    dec_len = base64_decode((const char*)encodedBuffer, enc_len, decodedBuffer);

    if (dec_len < 0) {
        printf("Failed to decode data.\n");
        goto fail;
    }

    //// Write to file
    hFile = CreateFileA("C:\\CTF\\test.jpg", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("File create failed: %lu\n", GetLastError());
        goto fail;
    }

    if (!WriteFile(hFile, decodedBuffer, dec_len, &written, NULL)) {
        printf("Write failed.\n");
        goto fail;
    }
    CloseHandle(hFile);
    VirtualFree(encodedBuffer, 0, MEM_RELEASE);
    VirtualFree(decodedBuffer, 0, MEM_RELEASE);
    return;

fail:
    VirtualFree(encodedBuffer, 0, MEM_RELEASE);
    VirtualFree(decodedBuffer, 0, MEM_RELEASE);
    ExitProcess(1);
}

int ProcessJpeg()
{
    LPCWSTR filepath = L"C:\\CTF\\test.jpg";
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    IWICImagingFactory* piFactory = NULL;
    IWICBitmapDecoder* piDecoder = NULL;

    // Create the COM imaging factory.
    if (SUCCEEDED(hr))
    {
        hr = CoCreateInstance(CLSID_WICImagingFactory,
            NULL, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&piFactory));
    }
    // Create the decoder.
    if (SUCCEEDED(hr))
    {
        hr = piFactory->CreateDecoderFromFilename(filepath, NULL, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, //For JPEG lossless decoding/encoding.
            &piDecoder);
    }

    // Variables used for encoding.
    IWICStream* piFileStream = NULL;
    IWICBitmapEncoder* piEncoder = NULL;
    IWICMetadataBlockWriter* piBlockWriter = NULL;
    IWICMetadataBlockReader* piBlockReader = NULL;

    WICPixelFormatGUID pixelFormat = { 0 };
    UINT count = 0;
    double dpiX, dpiY = 0.0;
    UINT width, height = 0;

    // Create a file stream.
    if (SUCCEEDED(hr))
    {
        hr = piFactory->CreateStream(&piFileStream);
    }

    // Initialize our new file stream.
    if (SUCCEEDED(hr))
    {
        hr = piFileStream->InitializeFromFilename(L"test2.jpg", GENERIC_WRITE);
    }

    // Create the encoder.
    if (SUCCEEDED(hr))
    {
        hr = piFactory->CreateEncoder(GUID_ContainerFormatJpeg, NULL, &piEncoder);
    }
    // Initialize the encoder
    if (SUCCEEDED(hr))
    {
        hr = piEncoder->Initialize(piFileStream, WICBitmapEncoderNoCache);
    }

    if (SUCCEEDED(hr))
    {
        hr = piDecoder->GetFrameCount(&count);
    }

    if (SUCCEEDED(hr))
    {
        // Process each frame of the image.
        for (UINT i = 0; i < count && SUCCEEDED(hr); i++)
        {
            // Frame variables.
            IWICBitmapFrameDecode* piFrameDecode = NULL;
            IWICBitmapFrameEncode* piFrameEncode = NULL;
            IWICMetadataQueryReader* piFrameQReader = NULL;
            IWICMetadataQueryWriter* piFrameQWriter = NULL;

            // Get and create the image frame.
            if (SUCCEEDED(hr))
            {
                hr = piDecoder->GetFrame(i, &piFrameDecode);
            }
            if (SUCCEEDED(hr))
            {
                hr = piEncoder->CreateNewFrame(&piFrameEncode, NULL);
            }

            // Initialize the encoder.
            if (SUCCEEDED(hr))
            {
                hr = piFrameEncode->Initialize(NULL);
            }
            // Get and set the size.
            if (SUCCEEDED(hr))
            {
                hr = piFrameDecode->GetSize(&width, &height);
            }
            if (SUCCEEDED(hr))
            {
                hr = piFrameEncode->SetSize(width, height);
            }
            // Get and set the resolution.
            if (SUCCEEDED(hr))
            {
                piFrameDecode->GetResolution(&dpiX, &dpiY);
            }
            if (SUCCEEDED(hr))
            {
                hr = piFrameEncode->SetResolution(dpiX, dpiY);
            }
            // Set the pixel format.
            if (SUCCEEDED(hr))
            {
                piFrameDecode->GetPixelFormat(&pixelFormat);
            }
            if (SUCCEEDED(hr))
            {
                hr = piFrameEncode->SetPixelFormat(&pixelFormat);
            }

            // Check that the destination format and source formats are the same.
            bool formatsEqual = FALSE;
            if (SUCCEEDED(hr))
            {
                GUID srcFormat;
                GUID destFormat;

                hr = piDecoder->GetContainerFormat(&srcFormat);
                if (SUCCEEDED(hr))
                {
                    hr = piEncoder->GetContainerFormat(&destFormat);
                }
                if (SUCCEEDED(hr))
                {
                    if (srcFormat == destFormat)
                        formatsEqual = true;
                    else
                        formatsEqual = false;
                }
            }

            if (SUCCEEDED(hr) && formatsEqual)
            {
                // Copy metadata using metadata block reader/writer.
                if (SUCCEEDED(hr))
                {
                    piFrameDecode->QueryInterface(IID_PPV_ARGS(&piBlockReader));
                }
                if (SUCCEEDED(hr))
                {
                    piFrameEncode->QueryInterface(IID_PPV_ARGS(&piBlockWriter));
                }
                if (SUCCEEDED(hr))
                {
                    piBlockWriter->InitializeFromBlockReader(piBlockReader);
                }
            }

            if (SUCCEEDED(hr))
            {
                hr = piFrameEncode->GetMetadataQueryWriter(&piFrameQWriter);
            }
            if (SUCCEEDED(hr))
            {
                hr = piFrameEncode->WriteSource(
                    static_cast<IWICBitmapSource*> (piFrameDecode),
                    NULL); // Using NULL enables JPEG loss-less encoding.
            }

            // Commit the frame.
            if (SUCCEEDED(hr))
            {
                hr = piFrameEncode->Commit();
            }

            if (piFrameDecode)
            {
                piFrameDecode->Release();
            }

            if (piFrameEncode)
            {
                piFrameEncode->Release();
            }

            if (piFrameQReader)
            {
                piFrameQReader->Release();
            }

            if (piFrameQWriter)
            {
                piFrameQWriter->Release();
            }
        }
    }

    if (SUCCEEDED(hr))
    {
        piEncoder->Commit();
    }

    if (SUCCEEDED(hr))
    {
        piFileStream->Commit(STGC_DEFAULT);
    }

    if (piFileStream)
    {
        piFileStream->Release();
    }
    if (piEncoder)
    {
        piEncoder->Release();
    }
    if (piBlockWriter)
    {
        piBlockWriter->Release();
    }
    if (piBlockReader)
    {
        piBlockReader->Release();
    }
    if (piDecoder) piDecoder->Release();
    if (piFactory) piFactory->Release();
    return 0;
}

int main()
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("Reward: %p\n", main);
    int choice;
    while (true)
    {
        PrintMenu();
        if (!read_int(&choice)) {
            printf("Invalid input.\n");
            continue;
        }

        switch (choice)
        {
        case 1:
            CreateHeap();
            break;
        case 2:
            DeleteHeap();
            break;
        case 3:
            ReadJpeg();
            ProcessJpeg();
            CleanupTempFiles();
            break;
        case 4:
            ExitProcess(1);
        default:
            cout << "Invalid option.\n";
            break;
        }
    }
    return 0;
}