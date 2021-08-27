/**
 * Copyright (C) 2017 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/
#ifndef DFMFILECONTROLLERFACTORY_H
#define DFMFILECONTROLLERFACTORY_H

#include "dfm-base/dfm_base_global.h"

#include <QStringList>

class DAbstractFileController;



class DFMFileControllerFactory
{
public:
    static QStringList keys();
    static DAbstractFileController *create(const QString &key);
};



#endif // DFMFILECONTROLLERFACTORY_H
