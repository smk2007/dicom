/*
*
*   application.h
*
*   CRTP application header
*
*/

#pragma once

#include "errors.h"

namespace Application
{
namespace Infrastructure
{

template <typename TDerived>
struct ApplicationBase
{
    static HRESULT Execute()
    {
        ApplicationBase<TDerived> applicationObject;
        RETURN_IF_FAILED(applicationObject.appbase_execute());
        return S_OK;
    }

private:
    HRESULT appbase_execute()
    {
        auto hr = appbase_run();

        if (FAILED(hr))
        {
            FAIL_FAST_IF_FAILED(shim().PrintHelp());
            return hr;
        }

        return S_OK;
    }

    HRESULT appbase_run()
    {
        RETURN_IF_FAILED(ProcessArguments());
        RETURN_IF_FAILED(shim().Run());
        return S_OK;
    }

public:
    // CRTP shims
    HRESULT Run() { return E_NOTIMPL; }
    bool ErrorOnInvalidParameter() { return true; }
    bool OnlyLogErrors() { return false; }
    HRESULT PrintHelp()
    {
        wprintf(L"\n>>>>> Sorry! There is no application provided help. <<<<<\n");
        return S_OK;
    };
    HRESULT GetLengthOfArgumentsToFollow(wchar_t*, unsigned* nArgsToFollow)
    {
        RETURN_HR_IF_NULL(E_POINTER, nArgsToFollow);
        *nArgsToFollow = 0;
        return E_INVALIDARG;
    };
    HRESULT ValidateArgument(const wchar_t*, bool* isValid)
    {
        RETURN_HR_IF_NULL(E_POINTER, isValid);
        *isValid = true;
        return S_OK;
    }

protected:
    HRESULT IsOptionSet(const wchar_t* optionName, bool* pIsSet)
    {
        RETURN_HR_IF_NULL(E_POINTER, optionName);
        RETURN_HR_IF_NULL(E_POINTER, pIsSet);
        *pIsSet = m_arguments.find(std::wstring(optionName)) != m_arguments.end();
        return S_OK;
    }

    template <unsigned TPos, typename T>
    HRESULT GetOptionParameterAt(const wchar_t* optionName, T* value)
    {
        RETURN_HR_IF_NULL(E_POINTER, optionName);
        RETURN_HR_IF_NULL(E_POINTER, value);
        auto foundIt = m_arguments.find(std::wstring(optionName));
        RETURN_HR_IF(E_FAIL, foundIt == m_arguments.end());
        auto params = foundIt->second;
        RETURN_HR_IF(E_INVALIDARG, TPos >= params.size());
        std::wistringstream oss(params.at(TPos));
        oss >> *value;
        RETURN_HR_IF(E_INVALIDARG, oss.fail());
        return S_OK;
    }

    HRESULT PrintInvalidArgument(bool isError, const wchar_t* pArgName, const wchar_t* pMessage)
    {
        std::wostringstream oss;
        const wchar_t* pwzMessageType = isError ? L"ERROR: " : L"WARN:  ";
        oss << pwzMessageType << pArgName << L": " << pMessage << std::endl;
        OutputDebugString(oss.str().c_str());
        wprintf(oss.str().c_str());
        return S_OK;
    }
private:
    TDerived& shim() { return *static_cast<TDerived*>(this); }



    HRESULT ProcessArguments()
    {
        int iArgs;
        auto args = CommandLineToArgvW(GetCommandLineW(), &iArgs);

        unsigned nArgs = static_cast<unsigned>(iArgs);
        for (unsigned i = 1; i < nArgs; i++)
        {
            wprintf(L"%ls ", args[i]);
        }

        wprintf(L"\n");

        nArgs = static_cast<unsigned>(iArgs);
        for (unsigned i = 1; i < nArgs;)
        {
            unsigned nArgsToFolllow;
            if (FAILED(shim().GetLengthOfArgumentsToFollow(args[i], &nArgsToFolllow)))
            {
                // Invalid parameter, refer to policy for failure behavior
                RETURN_IF_FAILED(
                    PrintInvalidArgument(
                        shim().ErrorOnInvalidParameter(),
                        args[i],
                        L"The parameter is not recognized."));
                
                RETURN_HR_IF(E_INVALIDARG, shim().ErrorOnInvalidParameter());
            }

            // Error if the parameter doesnt have the correct number of parameters following
            auto hasInsufficientArgs = nArgs - 1 - i < nArgsToFolllow;
            if (hasInsufficientArgs)
            {
                RETURN_IF_FAILED(
                    PrintInvalidArgument(
                        true,
                        args[i],
                        L"The parameter has insufficient arguments recognized."));
                return E_INVALIDARG;
            }

            std::vector<std::wstring> optionParameters;
            for (unsigned j = i + 1; j < i + 1 + nArgsToFolllow; j++)
            {
                optionParameters.emplace_back(std::wstring(args[j]));
            }

            m_arguments.emplace(std::wstring(args[i]), std::move(optionParameters));

            bool isValid = false;
            RETURN_IF_FAILED(shim().ValidateArgument(args[i], &isValid));
            RETURN_HR_IF_FALSE(E_INVALIDARG, isValid);

            i += nArgsToFolllow + 1;
        }
        return S_OK;
    }

private:
    std::map<std::wstring, std::vector<std::wstring>> m_arguments;
};

} // Infrastructure
} // Application