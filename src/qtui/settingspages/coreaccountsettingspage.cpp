/***************************************************************************
 *   Copyright (C) 2009 by the Quassel Project                             *
 *   devel@quassel-irc.org                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3.                                           *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "coreaccountsettingspage.h"

#include "client.h"
#include "clientsettings.h"
#include "coreaccountmodel.h"
#include "iconloader.h"

CoreAccountSettingsPage::CoreAccountSettingsPage(QWidget *parent)
: SettingsPage(tr("Misc"), tr("Core Accounts"), parent),
_lastAccountId(0),
_lastAutoConnectId(0)
{
  ui.setupUi(this);
  initAutoWidgets();
  ui.addAccountButton->setIcon(SmallIcon("list-add"));
  ui.editAccountButton->setIcon(SmallIcon("document-edit"));
  ui.deleteAccountButton->setIcon(SmallIcon("edit-delete"));

  _model = new CoreAccountModel(Client::coreAccountModel(), this);
  ui.accountView->setModel(_model);
  ui.autoConnectAccount->setModel(_model);

  connect(model(), SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)), SLOT(rowsAboutToBeRemoved(QModelIndex, int, int)));
  connect(model(), SIGNAL(rowsInserted(QModelIndex, int, int)), SLOT(rowsInserted(QModelIndex, int, int)));

  connect(ui.accountView->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), SLOT(setWidgetStates()));
  setWidgetStates();
}

void CoreAccountSettingsPage::load() {
  _model->update(Client::coreAccountModel());

  SettingsPage::load();
  ui.accountView->setCurrentIndex(model()->index(0, 0));
  ui.accountView->selectionModel()->select(model()->index(0, 0), QItemSelectionModel::Select);
  setWidgetStates();
}

void CoreAccountSettingsPage::save() {
  SettingsPage::save();
  Client::coreAccountModel()->update(_model);
  Client::coreAccountModel()->save();
  CoreAccountSettings s;
}

QVariant CoreAccountSettingsPage::loadAutoWidgetValue(const QString &widgetName) {
  if(widgetName == "autoConnectAccount") {
    CoreAccountSettings s;
    AccountId id = s.autoConnectAccount();
    if(!id.isValid())
      return QVariant();
    ui.autoConnectAccount->setCurrentIndex(model()->accountIndex(id).row());
    return id.toInt();
  }
  return SettingsPage::loadAutoWidgetValue(widgetName);
}

void CoreAccountSettingsPage::saveAutoWidgetValue(const QString &widgetName, const QVariant &v) {
  CoreAccountSettings s;
  if(widgetName == "autoConnectAccount") {
    AccountId id = _model->index(ui.autoConnectAccount->currentIndex(), 0).data(CoreAccountModel::AccountIdRole).value<AccountId>();
    s.setAutoConnectAccount(id);
    return;
  }
  SettingsPage::saveAutoWidgetValue(widgetName, v);
}

// TODO: Qt 4.6 - replace by proper rowsMoved() semantics
void CoreAccountSettingsPage::rowsAboutToBeRemoved(const QModelIndex &index, int start, int end) {
  _lastAutoConnectId = _lastAccountId = 0;
  if(index.isValid() || start != end)
    return;

  // the current index is removed, so remember it in case it's reinserted immediately afterwards
  AccountId id = model()->index(start, 0).data(CoreAccountModel::AccountIdRole).value<AccountId>();
  if(start == ui.accountView->currentIndex().row())
    _lastAccountId = id;
  if(start == ui.autoConnectAccount->currentIndex())
    _lastAutoConnectId = id;
}

void CoreAccountSettingsPage::rowsInserted(const QModelIndex &index, int start, int end) {
  if(index.isValid() || start != end)
    return;

  // check if the inserted index was just removed and select it in that case
  AccountId id = model()->index(start, 0).data(CoreAccountModel::AccountIdRole).value<AccountId>();
  if(id == _lastAccountId)
    ui.accountView->setCurrentIndex(model()->index(start, 0));
  if(id == _lastAutoConnectId)
    ui.autoConnectAccount->setCurrentIndex(start);
  _lastAccountId = _lastAutoConnectId = 0;
}

void CoreAccountSettingsPage::on_addAccountButton_clicked() {
  CoreAccountEditDlg dlg(CoreAccount(), this);
  if(dlg.exec() == QDialog::Accepted) {
    AccountId id =_model->createOrUpdateAccount(dlg.account());
    ui.accountView->setCurrentIndex(model()->accountIndex(id));
    widgetHasChanged();
  }
}

void CoreAccountSettingsPage::on_editAccountButton_clicked() {
  QModelIndex idx = ui.accountView->selectionModel()->currentIndex();
  if(!idx.isValid())
    return;

  CoreAccountEditDlg dlg(_model->account(idx), this);
  if(dlg.exec() == QDialog::Accepted) {
    AccountId id = _model->createOrUpdateAccount(dlg.account());
    ui.accountView->setCurrentIndex(model()->accountIndex(id));
    widgetHasChanged();
  }
}

void CoreAccountSettingsPage::on_deleteAccountButton_clicked() {
  if(!ui.accountView->selectionModel()->selectedIndexes().count())
    return;
  AccountId id = ui.accountView->selectionModel()->selectedIndexes().at(0).data(CoreAccountModel::AccountIdRole).value<AccountId>();
  if(id.isValid()) {
    model()->removeAccount(id);
    widgetHasChanged();
  }
}

void CoreAccountSettingsPage::setWidgetStates() {
  bool selected = ui.accountView->selectionModel()->selectedIndexes().count();

  ui.editAccountButton->setEnabled(selected);
  ui.deleteAccountButton->setEnabled(selected);
}

void CoreAccountSettingsPage::widgetHasChanged() {
  setChangedState(testHasChanged());
  setWidgetStates();
}

bool CoreAccountSettingsPage::testHasChanged() {
  if(!(*model() == *Client::coreAccountModel()))
    return true;

  return false;
}

/*****************************************************************************************
 * CoreAccountEditDlg
 *****************************************************************************************/
CoreAccountEditDlg::CoreAccountEditDlg(const CoreAccount &acct, QWidget *parent)
  : QDialog(parent)
{
  ui.setupUi(this);

  _account = acct;

  ui.hostName->setText(acct.hostName());
  ui.port->setValue(acct.port());
  ui.accountName->setText(acct.accountName());
  ui.user->setText(acct.user());
  ui.password->setText(acct.password());
  ui.rememberPassword->setChecked(acct.storePassword());
  ui.useProxy->setChecked(acct.useProxy());
  ui.proxyHostName->setText(acct.proxyHostName());
  ui.proxyPort->setValue(acct.proxyPort());
  ui.proxyType->setCurrentIndex(acct.proxyType() == QNetworkProxy::Socks5Proxy ? 0 : 1);
  ui.proxyUser->setText(acct.proxyUser());
  ui.proxyPassword->setText(acct.proxyPassword());

  if(acct.accountId().isValid())
    setWindowTitle(tr("Edit Core Account"));
  else
    setWindowTitle(tr("Add Core Account"));
}

CoreAccount CoreAccountEditDlg::account() {
  _account.setAccountName(ui.accountName->text().trimmed());
  _account.setHostName(ui.hostName->text().trimmed());
  _account.setPort(ui.port->value());
  _account.setUser(ui.user->text().trimmed());
  _account.setPassword(ui.password->text());
  _account.setStorePassword(ui.rememberPassword->isChecked());
  _account.setUseProxy(ui.useProxy->isChecked());
  _account.setProxyHostName(ui.proxyHostName->text().trimmed());
  _account.setProxyPort(ui.proxyPort->value());
  _account.setProxyType(ui.proxyType->currentIndex() == 0 ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy);
  _account.setProxyUser(ui.proxyUser->text().trimmed());
  _account.setProxyPassword(ui.proxyPassword->text());
  return _account;
}

void CoreAccountEditDlg::setWidgetStates() {
  bool ok = !ui.accountName->text().trimmed().isEmpty()
            && !ui.user->text().trimmed().isEmpty()
            && !ui.hostName->text().isEmpty();
  ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

void CoreAccountEditDlg::on_hostName_textChanged(const QString &text) {
  Q_UNUSED(text);
  setWidgetStates();
}

void CoreAccountEditDlg::on_accountName_textChanged(const QString &text) {
  Q_UNUSED(text);
  setWidgetStates();
}

void CoreAccountEditDlg::on_user_textChanged(const QString &text) {
  Q_UNUSED(text)
  setWidgetStates();
}
