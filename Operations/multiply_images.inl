#pragma once

namespace DCM
{
    namespace Operations
    {

        template <> struct Operation<OperationType::MultiplyImages>
        {
            Microsoft::WRL::ComPtr<ID3D11Buffer> m_spOutBuffer;

            // Input/output variables
            std::wstring m_inputFile;
            std::wstring m_inputFile2;
            std::wstring m_outputFile;

            bool m_fSaveFile;

            Operation(
                const std::wstring& inputFile,
                const std::wstring& inputFile2,
                const std::wstring& outputFile,
                bool fSaveFile = true) :
                m_inputFile(inputFile),
                m_inputFile2(inputFile2),
                m_outputFile(outputFile),
                m_fSaveFile(fSaveFile)
            {}

            ID3D11Buffer* GetBuffer()
            {
                return m_spOutBuffer.Get();
            }

            HRESULT Run(Application::Infrastructure::DeviceResources& resources)
            {
                UNREFERENCED_PARAMETER(resources);
                wchar_t pwzFileName[MAX_PATH + 1];
                RETURN_HR_IF(E_FAIL, 0 == GetModuleFileName(NULL, pwzFileName, MAX_PATH + 1));
                std::wstring shaderPath(pwzFileName);
                shaderPath.erase(shaderPath.begin() + shaderPath.find_last_of(L'\\') + 1, shaderPath.end());
                shaderPath += L"Shaders\\multiply_images.hlsl";

                // Create the shader
                Microsoft::WRL::ComPtr<ID3D11ComputeShader> spComputeShader;
                RETURN_IF_FAILED(resources.CreateComputeShader(shaderPath.c_str(), "CSMain", &spComputeShader));

                auto convertFile1Op = MakeOperation<OperationType::ConvertToFloat>(m_inputFile);
                convertFile1Op->Run(resources);
                auto pBuffer1 = convertFile1Op->GetBuffer();

                auto convertFile2Op = MakeOperation<OperationType::ConvertToFloat>(m_inputFile2);
                convertFile2Op->Run(resources);
                auto pBuffer2 = convertFile2Op->GetBuffer();

                RETURN_HR_IF_FALSE(E_FAIL, convertFile1Op->GetRows() == convertFile2Op->GetRows());
                RETURN_HR_IF_FALSE(E_FAIL, convertFile1Op->GetColumns() == convertFile2Op->GetColumns());

                Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spShaderResourceView1;
                RETURN_IF_FAILED(resources.CreateStructuredBufferSRV(pBuffer1,
                    &spShaderResourceView1));

                Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spShaderResourceView2;
                RETURN_IF_FAILED(resources.CreateStructuredBufferSRV(pBuffer2,
                    &spShaderResourceView2));

                std::vector<ID3D11ShaderResourceView*> sharedResourceViews =
                { spShaderResourceView1.Get(), spShaderResourceView2.Get() };

                auto width = convertFile1Op->GetColumns();
                auto height = convertFile1Op->GetRows();

                Log(L"Creating resources for output buffer: (%d, %d)", height, width);
                std::vector<float> zeroOutBuffer(width * height, 0.f);
                FAIL_FAST_IF_FAILED(resources.CreateStructuredBuffer(
                    sizeof(float) /* size of item */,
                    width * height /* num items */,
                    &zeroOutBuffer[0]/* data */,
                    &m_spOutBuffer));

                Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> spOutBufferUnorderedAccessView;
                FAIL_FAST_IF_FAILED(
                    resources.CreateStructuredBufferUAV(
                        m_spOutBuffer.Get(),
                        &spOutBufferUnorderedAccessView));

                std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs
                    { spOutBufferUnorderedAccessView };

                Log(L"Created resources for output buffer.");
                resources.RunComputeShader(spComputeShader.Get(),
                    nullptr, 2, &sharedResourceViews[0],
                    uavs, width * height, 1, 1);

                if (m_fSaveFile)
                {
                    RETURN_IF_FAILED(
                        SaveToFile(
                            resources,
                            m_spOutBuffer.Get(),
                            width,
                            height,
                            sizeof(float),
                            m_outputFile.c_str()));
                }
                return S_OK;
            }
        };

        template <> void inline LogOperation<OperationType::MultiplyImages>() { Log(L"[OperationType::AverageImages]"); }

    } // Operations
} // DCM