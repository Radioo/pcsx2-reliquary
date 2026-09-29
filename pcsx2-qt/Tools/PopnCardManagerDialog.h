// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "pcsx2/FireWire/Devices/PopnCard.h"

#include <QtWidgets/QDialog>

#include <optional>
#include <string>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;

class PopnCardManagerDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit PopnCardManagerDialog(QWidget* parent);
	~PopnCardManagerDialog() override;

	static void migrateLegacySettings();
	static QString describeCard(const std::string& name);

private:
	void refreshList(const std::string& select_name);
	void showSelectedCard();
	std::optional<std::string> selectedName() const;
	void saveSelected(const PopnCard::Bytes& card);

	void onNewClicked();
	void onImportClicked();
	void onOpenFolderClicked();
	void onRenameClicked();
	void onCopyClicked();
	void onExportClicked();
	void onDeleteClicked();
	void onRepairClicked();
	void onCardDataChanged(int index);

	QListWidget* m_list = nullptr;
	QStackedWidget* m_pages = nullptr;
	QLabel* m_face = nullptr;
	QLabel* m_title = nullptr;
	QLabel* m_game = nullptr;
	QLabel* m_state = nullptr;
	QPushButton* m_repair = nullptr;
	QLabel* m_number = nullptr;
	QComboBox* m_card_data = nullptr;
	QLabel* m_card_data_value = nullptr;
	QLabel* m_readers = nullptr;
	QLabel* m_twins = nullptr;
	QLabel* m_changed = nullptr;
	QPushButton* m_copy = nullptr;
	QTableWidget* m_bytes = nullptr;
};
