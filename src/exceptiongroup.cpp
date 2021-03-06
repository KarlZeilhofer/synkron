/*******************************************************************
 This file is part of Synkron
 Copyright (C) 2005-2011 Matus Tomlein (matus.tomlein@gmail.com)

 Synkron is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public Licence
 as published by the Free Software Foundation; either version 2
 of the Licence, or (at your option) any later version.

 Synkron is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public Licence for more details.

 You should have received a copy of the GNU General Public Licence
 along with Synkron; if not, write to the Free Software Foundation,
 Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
********************************************************************/

#include "exceptiongroup.h"

#include "settings.h"

#include <QStringList>

ExceptionGroup::ExceptionGroup()
{
    settings_map = new QMap<QString, QVariant>;
    data = new QStringList();
}

ExceptionGroup::~ExceptionGroup()
{
    delete settings_map;
    delete data;
}

/**
  * Inserts the key and value to settings_map.
  */
void ExceptionGroup::setValue(const QString & key, const QVariant & value)
{
    settings_map->insert(key, value);
}

/**
  * Returns value for the key from the settings_map.
  */
const QVariant ExceptionGroup::value(const QString & key)
{
    return settings_map->value(key);
}

/**
  * Returns count of the data string list of items.
  */
int ExceptionGroup::count()
{
    return data->count();
}

/**
  * Returns string at position in the data string list.
  */
const QString & ExceptionGroup::at(int i)
{
    return data->at(i);
}

/**
  * Adds a new string item to data
  */
bool ExceptionGroup::addItem(const QString & item)
{
    if (data->contains(item)) return false;
    data->append(item);
    return true;
}

/**
  * Removes all occurences of the item in data.
  */
void ExceptionGroup::removeItem(const QString & item)
{
    data->removeAll(item);
}

void ExceptionGroup::removeAt(int i)
{
    data->removeAt(i);
}

/**
  * Saves settings at quit. Triggered from outside.
  */
void ExceptionGroup::save(Settings * settings)
{
    settings->beginGroup(settings_map->value("type").toString());
    settings->remove("");

    QMapIterator<QString, QVariant> i(*settings_map);
    while (i.hasNext()) {
        i.next();
        settings->setValue(i.key(), i.value());
    }

    settings->setValue("data", data->join(";"));

    settings->endGroup();
}

/**
  * Loads settings. Run at start up, triggered from outside.
  */
void ExceptionGroup::load(Settings * settings)
{
    settings->beginGroup(settings_map->value("type").toString());

    QStringList keys = settings->childKeys();
    for (int i = 0; i < keys.count(); ++i) {
        if (keys.at(i) == "data")
            if (!settings->value(keys.at(i)).toString().isEmpty())
                data->append(settings->value(keys.at(i)).toString().split(";"));
        else
            settings_map->insert(keys.at(i), settings->value(keys.at(i)));
    }

    settings->endGroup();
}

/**
  * Appends i_data to data.
  */
void ExceptionGroup::importData(const QStringList * i_data)
{
    data->append(*i_data);
}
