#pragma once

#include "TSEPlugin.h"
#include <gui/CheckBox.h>
#include <gui/GridComposer.h>
#include <gui/GridLayout.h>
#include <gui/Label.h>
#include <gui/View.h>

class ViewOptions : public gui::View
{
protected:
    gui::Label _lblMode;
    gui::CheckBox _autoDetect;
    gui::CheckBox _writeComments;
    gui::GridLayout _gl;
    ConversionOptions _options;

public:
    ViewOptions()
    : _lblMode("Format:")
    , _autoDetect("Auto-detect input format")
    , _writeComments("Write comments in output")
    , _gl(3, 2)
    {
        _autoDetect.setChecked(true);
        _writeComments.setChecked(true);

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblMode) << _autoDetect;
        gc.appendRow(_writeComments, 0);
        setLayout(&_gl);
    }

    const ConversionOptions& getOptions()
    {
        _options.inputKind = _autoDetect.isChecked() ? InputKind::Auto : InputKind::Matpower;
        _options.writeComments = _writeComments.isChecked();
        return _options;
    }
};
