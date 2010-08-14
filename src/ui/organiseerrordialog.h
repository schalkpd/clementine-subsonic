/* This file is part of Clementine.

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ORGANISEERRORDIALOG_H
#define ORGANISEERRORDIALOG_H

#include <QDialog>

#include "core/song.h"

class Ui_OrganiseErrorDialog;

class OrganiseErrorDialog : public QDialog {
  Q_OBJECT

public:
  OrganiseErrorDialog(QWidget* parent = 0);
  ~OrganiseErrorDialog();

  enum OperationType {
    Type_Copy,
    Type_Delete,
  };

  void Show(OperationType type, const SongList& songs_with_errors);
  void Show(OperationType type, const QStringList& files_with_errors);

private:
  Ui_OrganiseErrorDialog* ui_;
};

#endif // ORGANISEERRORDIALOG_H