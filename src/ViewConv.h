#pragma once

#include "ConverterCore.h"
#include "ViewOptions.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <gui/Button.h>
#include <gui/FileDialog.h>
#include <gui/GridComposer.h>
#include <gui/GridLayout.h>
#include <gui/HorizontalLayout.h>
#include <gui/Label.h>
#include <gui/LineEdit.h>
#include <gui/ProgressIndicator.h>
#include <gui/TextEdit.h>
#include <gui/Thread.h>
#include <gui/View.h>
#include <fo/FileOperations.h>
#include <string>
#include <thread>
#include <thread/Thread.h>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <commdlg.h>
#endif

class ViewConv : public gui::View
{
protected:
    sc::IPlugin* _pIPlugin;
    sc::IPlugin::CallBack _onComplete;
    ViewOptions* _pViewOptions = nullptr;

    gui::Label _lblFnIn;
    gui::LineEdit _editFnIn;
    gui::Button _btnSelectInFn;
    gui::Label _lblFnOut;
    gui::LineEdit _editFnOut;
    gui::Button _btnSelectOutFn;
    gui::Label _lblStatus;
    gui::LineEdit _editStatus;
    gui::Label _lblLog;
    gui::ProgressIndicator _progress;
    gui::TextEdit _preview;
    gui::Button _btnConvert;
    gui::Button _btnCancel;
    gui::HorizontalLayout _hlButtons;
    gui::GridLayout _gl;

    std::thread _worker;
    std::atomic_bool _running = false;
    std::atomic_bool _cancelRequested = false;
    ConversionResult _lastResult;
    double _progressValue = 0.0;
    td::String _progressText;
    gui::AsyncFn _asyncProgress;
    gui::AsyncFn _asyncFinished;
    td::UINT4 _wndID = 100;

protected:
    static std::string toStdString(const td::String& s)
    {
        return std::string(s.c_str());
    }

    static td::String makeDefaultOutputFileName(const td::String& inputFileName)
    {
        std::string path(inputFileName.c_str());
        const size_t slashPos = path.find_last_of("\\/");
        const size_t dotPos = path.find_last_of('.');
        const bool hasExtension = dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos);

        bool matpowerInput = false;
        if (hasExtension)
        {
            std::string extension = path.substr(dotPos);
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char ch) { return (char) std::tolower(ch); });
            matpowerInput = extension == ".m";
        }
        if (hasExtension)
            path.erase(dotPos);

        if (matpowerInput)
            path += ".dmodl";
        else
            path += "_complex.mtx";

        return td::String(path.c_str());
    }

    static td::String getFileNameOnly(const td::String& filePath)
    {
        std::string path(filePath.c_str());
        const size_t slashPos = path.find_last_of("\\/");
        if (slashPos == std::string::npos)
            return filePath;
        return td::String(path.substr(slashPos + 1).c_str());
    }

    static bool hasFileExtension(const td::String& filePath)
    {
        std::string path(filePath.c_str());
        const size_t slashPos = path.find_last_of("\\/");
        const size_t dotPos = path.find_last_of('.');
        return dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos);
    }

    static bool isMatpowerFileName(const td::String& filePath)
    {
        std::string path(filePath.c_str());
        const size_t slashPos = path.find_last_of("\\/");
        const size_t dotPos = path.find_last_of('.');
        if (dotPos == std::string::npos || (slashPos != std::string::npos && dotPos < slashPos))
            return false;

        std::string extension = path.substr(dotPos);
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char ch) { return (char) std::tolower(ch); });
        return extension == ".m";
    }

    static td::String ensureDefaultOutputExtension(const td::String& outputFileName, const td::String& inputFileName)
    {
        if (outputFileName.isEmpty() || hasFileExtension(outputFileName))
            return outputFileName;

        td::String normalized(outputFileName);
        normalized += isMatpowerFileName(inputFileName) ? ".dmodl" : ".mtx";
        return normalized;
    }

    static bool browseOutputFile(td::String& fileName, const td::String& inputFileName)
    {
#ifdef _WIN32
        char buffer[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = sizeof(buffer);

        if (isMatpowerFileName(inputFileName))
        {
            ofn.lpstrFilter = "dTwin digital model (*.dmodl)\0*.dmodl\0All files (*.*)\0*.*\0";
            ofn.lpstrDefExt = "dmodl";
        }
        else
        {
            ofn.lpstrFilter = "Sparse matrix (*.mtx)\0*.mtx\0All files (*.*)\0*.*\0";
            ofn.lpstrDefExt = "mtx";
        }

        ofn.lpstrTitle = "Browse output file";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetSaveFileNameA(&ofn))
            return false;

        fileName = td::String(buffer);
        return true;
#else
        (void) fileName;
        (void) inputFileName;
        return false;
#endif
    }

    void updateProgress()
    {
        _progress.setValue(_progressValue);
        _editStatus = _progressText;
    }

    void conversionFinished()
    {
        if (_worker.joinable())
            _worker.join();

        _running = false;
        _btnConvert.setTitle("Convert");
        _btnCancel.disable();

        _editStatus = td::String(_lastResult.message.c_str());
        td::String logText;
        if (_lastResult.ok)
        {
            logText = td::String(_lastResult.preview.c_str());
        }
        else
        {
            logText = "ERROR:\n";
            logText += td::String(_lastResult.message.c_str());
        }
        _preview.setText(logText);

        if (_lastResult.ok && _lastResult.openInDTwin && _pIPlugin)
            _onComplete(_pIPlugin);
    }

    void workerMethod(std::string inputPath, std::string outputPath, ConversionOptions options)
    {
        _lastResult = convertToComplexCoordinates(inputPath, outputPath, options, _cancelRequested,
            [this](double value, const std::string& text)
            {
                _progressValue = value;
                _progressText = td::String(text.c_str());
                asyncCall(&_asyncProgress);
            });

        asyncCall(&_asyncFinished);
    }

    void startConversion()
    {
        if (_running)
            return;

        td::String inputFileName = _editFnIn.getText();
        td::String outputFileName = ensureDefaultOutputExtension(_editFnOut.getText(), inputFileName);

        if (inputFileName.isEmpty())
        {
            _editStatus = "ERROR: odaberi ulazni fajl.";
            _preview.setText("ERROR:\nOdaberi ulazni fajl koji treba konvertovati.");
            return;
        }
        if (!fo::fileExists(inputFileName))
        {
            _editStatus = "ERROR: ulazni fajl ne postoji.";
            _preview.setText("ERROR:\nUlazni fajl ne postoji.");
            return;
        }
        if (outputFileName.isEmpty())
        {
            _editStatus = "ERROR: odaberi izlazni fajl.";
            _preview.setText("ERROR:\nOdaberi gdje ce se snimiti novonastali fajl.");
            return;
        }
        _editFnOut = outputFileName;
        if (!_pViewOptions)
        {
            _editStatus = "ERROR: opcije nisu povezane.";
            _preview.setText("ERROR:\nOpcije konverzije nisu povezane.");
            return;
        }

        _cancelRequested = false;
        _running = true;
        _progressValue = 0.0;
        _progressText = "Pokrecem konverziju";
        updateProgress();
        _preview.clean();
        _btnConvert.setTitle("Running");
        _btnCancel.enable();

        ConversionOptions options = _pViewOptions->getOptions();
        _worker = std::thread(&ViewConv::workerMethod, this, toStdString(inputFileName), toStdString(outputFileName), options);
    }

    bool onClick(gui::Button* pBtn) override
    {
        if (pBtn == &_btnConvert)
        {
            startConversion();
            return true;
        }
        if (pBtn == &_btnCancel)
        {
            _cancelRequested = true;
            _editStatus = "Prekidam nakon trenutnog koraka...";
            return true;
        }
        return false;
    }

    void handleUserActions()
    {
        _btnSelectInFn.onClick([this] {
            gui::OpenFileDialog::show(this, "Open input file",
                {{"MATPOWER case", "*.m"}, {"Sparse matrix", "*.mtx"}, {"Text file", "*.txt"}},
                _wndID + 1000, [this](gui::FileDialog* pDlg)
            {
                if (pDlg->getStatus() == gui::FileDialog::Status::OK)
                {
                    td::String fileName = pDlg->getFileName();
                    if (!fileName.isEmpty())
                    {
                        _editFnIn = fileName;
                        _btnSelectOutFn.enable();
                        _editStatus = "Ulazni fajl je odabran.";
                    }
                }
            });
        });

        _btnSelectOutFn.onClick([this] {
            td::String fileName;
            if (browseOutputFile(fileName, _editFnIn.getText()))
            {
                fileName = ensureDefaultOutputExtension(fileName, _editFnIn.getText());
                if (!fileName.isEmpty())
                {
                    _editFnOut = fileName;
                    _editFnOut.setFocus();
                    _editStatus = "Izlazni fajl je odabran.";
                }
            }
        });
    }

public:
    ViewConv(sc::IPlugin* pIPlugin, const sc::IPlugin::CallBack& onComplete)
    : _pIPlugin(pIPlugin)
    , _onComplete(onComplete)
    , _lblFnIn("Input:")
    , _btnSelectInFn("Open")
    , _lblFnOut("Output:")
    , _btnSelectOutFn("Browse")
    , _lblStatus("Status:")
    , _lblLog("Conversion log / errors:")
    , _btnConvert("Convert")
    , _btnCancel("Cancel")
    , _hlButtons(3)
    , _gl(7, 3)
    , _asyncProgress(std::bind(&ViewConv::updateProgress, this))
    , _asyncFinished(std::bind(&ViewConv::conversionFinished, this))
    {
        _editStatus.setAsReadOnly();
        _preview.setAsReadOnly();
        _btnSelectOutFn.disable();
        _btnCancel.disable();

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblFnIn) << _editFnIn << _btnSelectInFn;
        gc.appendRow(_lblFnOut) << _editFnOut << _btnSelectOutFn;
        gc.appendRow(_lblStatus); gc.appendCol(_editStatus, 0);
        gc.appendRow(_progress, 0);
        gc.appendRow(_lblLog, 0);
        gc.appendRow(_preview, 0);
        _hlButtons.appendSpacer() << _btnConvert << _btnCancel;
        gc.appendRow(_hlButtons, 0);

        setLayout(&_gl);
        handleUserActions();
    }

    ~ViewConv()
    {
        _cancelRequested = true;
        if (_worker.joinable())
            _worker.join();
    }

    void setOptions(ViewOptions* pViewOptions)
    {
        _pViewOptions = pViewOptions;
    }

    td::String getOutFileName() const
    {
        return _editFnOut.getText();
    }
};
