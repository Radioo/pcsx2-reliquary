// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Tools/PopnCardManagerDialog.h"
#include "QtHost.h"
#include "QtUtils.h"

#include "pcsx2/Config.h"
#include "pcsx2/GameList.h"
#include "pcsx2/INISettingsInterface.h"

#include "common/FileSystem.h"
#include "common/Path.h"

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QLocale>
#include <QtCore/QSignalBlocker>
#include <QtCore/QUrl>
#include <QtGui/QLinearGradient>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <array>
#include <span>
#include <vector>

namespace
{
	constexpr const char* GAME_SETTINGS_PATTERN = "*.ini";
	constexpr char GAME_SETTINGS_SERIAL_SEPARATOR = '_';
	constexpr int BYTE_COLUMNS = 16;
	constexpr int BYTE_COLUMN_WIDTH = 30;
	constexpr int FIELD_ALPHA = 80;
	constexpr QSize LIST_FACE_SIZE(88, 56);
	constexpr QSize DETAIL_FACE_SIZE(300, 189);
	constexpr const char* BLANK_NUMBER = "0000000000000000";
	constexpr int NEW_CARD_DIALOG_WIDTH = 440;
	constexpr int NUMBER_FIELD_PADDING = 24;

	struct Bonus
	{
		u8 value;
		const char* label;
	};

	constexpr std::array<Bonus, 22> BONUSES = {{
		{0, "None"},
		{1, "LOVE FIRE"},
		{2, "TOON MANIAC"},
		{3, "Cry Out"},
		{4, "HONEまで トゥナイト"},
		{5, "Tap'n! Slap'n! Pop'n Music!"},
		{6, "WITHOUT YOU AROUND"},
		{7, "映画「SICILLIANA」のテーマ"},
		{8, "宇宙船Q-Mex"},
		{9, "ostin-art"},
		{10, "クチビル"},
		{11, "power plant"},
		{12, "In a Grow"},
		{13, "Character: cats"},
		{14, "Character: yoshimoto"},
		{15, "Character: honerock"},
		{16, "Character: red"},
		{17, "Character: comoba"},
		{18, "Hi-SPEED x6 / x8"},
		{19, "CHARA-POP / STAGE-POP"},
		{20, "SUPER RANDOM"},
		{21, "Unlocks everything"},
	}};

	constexpr std::array<Bonus, 33> POPN10_CARD_DATA = {{
		{0, "None, MOON"},
		{1, "Character: ningen, HI-SPEED x6, SUN"},
		{2, "Character: radio, SUPER RANDOM, MOON"},
		{3, "Character: gacha, HI-SPEED x6, SUN"},
		{4, "Character: shoten, SUPER RANDOM, MOON"},
		{5, "Character: rider, HI-SPEED x6, SUN"},
		{6, "Character: curry, SUPER RANDOM, MOON"},
		{7, "Character: seken, HI-SPEED x6, SUN"},
		{8, "Character: urusei, SUPER RANDOM, MOON"},
		{9, "Character: sazae, HI-SPEED x6, SUN"},
		{10, "Character: haige, SUPER RANDOM, MOON"},
		{11, "SP: every guest character, HI-SPEED x6 and SUPER RANDOM, SUN"},
		{12, "SP: every guest character, HI-SPEED x6 and SUPER RANDOM, MOON"},
		{13, "SP: every guest character, HI-SPEED x6 and SUPER RANDOM, SUN"},
		{14, "SP: every guest character, HI-SPEED x6 and SUPER RANDOM, MOON"},
		{15, "SP: every guest character, HI-SPEED x6 and SUPER RANDOM, SUN"},
		{16, "SP: every guest character, HI-SPEED x6 and SUPER RANDOM, MOON"},
		{17, "SP: every guest character, HI-SPEED x6 and SUPER RANDOM, SUN"},
		{18, "SP: every guest character, HI-SPEED x6 and SUPER RANDOM, MOON"},
		{19, "SP: every guest character, HI-SPEED x6 and SUPER RANDOM, SUN"},
		{20, "SP: every guest character, HI-SPEED x6 and SUPER RANDOM, MOON"},
		{21, "HI-SPEED x6, SUN"},
		{22, "SUPER RANDOM, MOON"},
		{23, "HI-SPEED x6, SUN"},
		{24, "SUPER RANDOM, MOON"},
		{25, "HI-SPEED x6, SUN"},
		{26, "SUPER RANDOM, MOON"},
		{27, "HI-SPEED x6, SUN"},
		{28, "SUPER RANDOM, MOON"},
		{29, "HI-SPEED x6, SUN"},
		{30, "SUPER RANDOM, MOON"},
		{31, "Character: cup_9, SUN"},
		{32, "Character: cup_9, MOON"},
	}};

	struct Field
	{
		u32 from;
		u32 to;
		QColor color;
		const char* name;
	};

	const std::array<Field, 6> FIELDS = {{
		{0, PopnCard::TYPE_SIZE - 1, QColor(0x7b, 0x61, 0xe8), QT_TRANSLATE_NOOP("PopnCardManagerDialog", "Type")},
		{PopnCard::TYPE_CHECKSUM_OFFSET, PopnCard::TYPE_CHECKSUM_OFFSET, QColor(0x8a, 0x87, 0x9a), QT_TRANSLATE_NOOP("PopnCardManagerDialog", "Checksum")},
		{PopnCard::CARD_DATA_OFFSET, PopnCard::CARD_DATA_OFFSET, QColor(0xc7, 0x7a, 0x00), QT_TRANSLATE_NOOP("PopnCardManagerDialog", "Card data")},
		{PopnCard::NUMBER_OFFSET, PopnCard::NUMBER_OFFSET + PopnCard::NUMBER_SIZE - 1, QColor(0x10, 0x8a, 0x8a), QT_TRANSLATE_NOOP("PopnCardManagerDialog", "Number")},
		{PopnCard::NUMBER_OFFSET + PopnCard::NUMBER_SIZE, PopnCard::DATA_CHECKSUM_OFFSET - 1, QColor(0xc9, 0xc6, 0xd4), QT_TRANSLATE_NOOP("PopnCardManagerDialog", "Unused")},
		{PopnCard::DATA_CHECKSUM_OFFSET, PopnCard::RECORD_SIZE - 1, QColor(0x8a, 0x87, 0x9a), QT_TRANSLATE_NOOP("PopnCardManagerDialog", "Checksum")},
	}};

	struct GameCardSetting
	{
		std::string ini_path;
		std::string serial;
		u32 crc;
		std::string card_name;
	};

	QString tr(const char* text)
	{
		return PopnCardManagerDialog::tr(text);
	}

	void fillCardDataChoices(QComboBox* combo, PopnCard::Game game)
	{
		combo->clear();
		const std::span<const Bonus> choices = game == PopnCard::Game::Popn10 ? std::span<const Bonus>(POPN10_CARD_DATA) : std::span<const Bonus>(BONUSES);
		for (const Bonus& bonus : choices)
			combo->addItem(QStringLiteral("%1 - %2").arg(static_cast<int>(bonus.value)).arg(tr(bonus.label)), static_cast<int>(bonus.value));
	}

	QString gameName(PopnCard::Game game)
	{
		switch (game)
		{
			case PopnCard::Game::Popn9:
				return tr("pop'n 9");
			case PopnCard::Game::Popn10:
				return tr("pop'n 10");
			case PopnCard::Game::Common:
				return tr("Common card");
			default:
				return tr("Unknown card");
		}
	}

	bool isDamaged(const PopnCard::Info& info)
	{
		return !info.type_checksum_ok || !info.data_checksum_ok;
	}

	bool isRegistered(PopnCard::Type type)
	{
		return type == PopnCard::Type::Popn9Registered || type == PopnCard::Type::Popn10Registered;
	}

	bool isBlank(PopnCard::Type type)
	{
		return type == PopnCard::Type::Popn9Blank || type == PopnCard::Type::Popn10Blank;
	}

	QString stateName(const PopnCard::Info& info)
	{
		if (isDamaged(info))
			return tr("Damaged");

		switch (info.type)
		{
			case PopnCard::Type::Popn9Registered:
			case PopnCard::Type::Popn10Registered:
				return tr("Registered");
			case PopnCard::Type::Popn9Blank:
			case PopnCard::Type::Popn10Blank:
				return tr("Blank");
			case PopnCard::Type::Popn9Death:
				return tr("DEATH");
			case PopnCard::Type::Popn9LocationTestBlank:
			case PopnCard::Type::Popn9LocationTestRegistered:
			case PopnCard::Type::Popn10LocationTestBlank:
			case PopnCard::Type::Popn10LocationTestRegistered:
				return tr("Location test");
			case PopnCard::Type::KonamiCommon:
			case PopnCard::Type::EamusementCommon:
				return tr("Common");
			default:
				return tr("Unknown");
		}
	}

	QString groupNumber(const std::string& number)
	{
		QString grouped;
		for (size_t i = 0; i < number.size(); i++)
		{
			if (i != 0 && (i % 4) == 0)
				grouped += QLatin1Char(' ');
			grouped += QLatin1Char(number[i]);
		}
		return grouped;
	}

	QPixmap renderFace(const PopnCard::Info& info, const QString& name, const QSize& size, qreal dpr, bool detailed)
	{
		QPixmap pixmap(size * dpr);
		pixmap.setDevicePixelRatio(dpr);
		pixmap.fill(Qt::transparent);

		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		const QRectF rect(0, 0, size.width(), size.height());
		const qreal height = rect.height();

		QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
		if (isDamaged(info) || info.game == PopnCard::Game::Unknown || info.game == PopnCard::Game::Common)
		{
			gradient.setColorAt(0.0, QColor(0x8d, 0x8a, 0x9c));
			gradient.setColorAt(1.0, QColor(0x6f, 0x6c, 0x80));
		}
		else if (info.game == PopnCard::Game::Popn10)
		{
			gradient.setColorAt(0.0, QColor(0xff, 0x9a, 0x2e));
			gradient.setColorAt(0.6, QColor(0xf0, 0x50, 0x6e));
			gradient.setColorAt(1.0, QColor(0xc9, 0x3d, 0x9c));
		}
		else
		{
			gradient.setColorAt(0.0, QColor(0x1b, 0x8f, 0xd9));
			gradient.setColorAt(0.55, QColor(0x3f, 0x5f, 0xd8));
			gradient.setColorAt(1.0, QColor(0x7a, 0x4f, 0xd6));
		}

		QPainterPath shape;
		shape.addRoundedRect(rect, height * 0.1, height * 0.1);
		painter.fillPath(shape, gradient);
		painter.setClipPath(shape);
		painter.fillRect(QRectF(0, height * 0.69, rect.width(), height * 0.17), QColor(0x2a, 0x25, 0x33, 0xe0));

		painter.setPen(Qt::white);
		QFont title_font = painter.font();
		title_font.setBold(true);
		title_font.setPixelSize(std::max(8, static_cast<int>(height * 0.16)));
		painter.setFont(title_font);
		painter.drawText(QRectF(height * 0.1, height * 0.08, rect.width(), height * 0.25), Qt::AlignLeft | Qt::AlignTop, gameName(info.game));

		if (!detailed)
			return pixmap;

		QFont small_font = painter.font();
		small_font.setBold(false);
		small_font.setPixelSize(static_cast<int>(height * 0.075));
		painter.setFont(small_font);
		painter.drawText(QRectF(0, height * 0.1, rect.width() - height * 0.1, height * 0.2), Qt::AlignRight | Qt::AlignTop, stateName(info));
		painter.drawText(QRectF(height * 0.1, height * 0.87, rect.width(), height * 0.12), Qt::AlignLeft | Qt::AlignVCenter, name);

		QFont number_font(QStringLiteral("monospace"));
		number_font.setStyleHint(QFont::Monospace);
		number_font.setPixelSize(static_cast<int>(height * 0.1));
		painter.setFont(number_font);
		painter.drawText(QRectF(height * 0.1, height * 0.5, rect.width(), height * 0.16), Qt::AlignLeft | Qt::AlignVCenter,
			isBlank(info.type) ? QStringLiteral("---- ---- ---- ----") : groupNumber(info.number));
		return pixmap;
	}

	std::vector<GameCardSetting> readGameCardSettings()
	{
		FileSystem::FindResultsArray results;
		FileSystem::FindFiles(EmuFolders::GameSettings.c_str(), GAME_SETTINGS_PATTERN, FILESYSTEM_FIND_FILES, &results);

		std::vector<GameCardSetting> settings;
		for (const FILESYSTEM_FIND_DATA& result : results)
		{
			INISettingsInterface ini(result.FileName);
			if (!ini.Load())
				continue;

			const std::string title(Path::GetFileTitle(result.FileName));
			const size_t separator = title.rfind(GAME_SETTINGS_SERIAL_SEPARATOR);
			GameCardSetting setting;
			setting.ini_path = result.FileName;
			setting.serial = separator == std::string::npos ? std::string() : title.substr(0, separator);
			setting.crc = static_cast<u32>(std::strtoul(title.substr(separator == std::string::npos ? 0 : separator + 1).c_str(), nullptr, 16));
			setting.card_name = ini.GetStringValue(PopnCard::SETTINGS_SECTION, PopnCard::CARD_FILE_KEY, "");
			settings.push_back(std::move(setting));
		}
		return settings;
	}

	QString gameTitle(const GameCardSetting& setting)
	{
		auto lock = GameList::GetLock();
		const GameList::Entry* entry = GameList::GetEntryBySerialAndCRC(setting.serial, setting.crc);
		return entry ? QString::fromStdString(entry->GetTitle()) : QString::fromStdString(setting.serial);
	}

	void replaceCardInGameSettings(const std::string& old_name, const std::string& new_name)
	{
		bool changed = false;
		for (const GameCardSetting& setting : readGameCardSettings())
		{
			if (setting.card_name != old_name)
				continue;

			INISettingsInterface ini(setting.ini_path);
			if (!ini.Load())
				continue;

			if (new_name.empty())
				ini.DeleteValue(PopnCard::SETTINGS_SECTION, PopnCard::CARD_FILE_KEY);
			else
				ini.SetStringValue(PopnCard::SETTINGS_SECTION, PopnCard::CARD_FILE_KEY, new_name.c_str());
			changed |= ini.Save();
		}

		if (changed)
			g_emu_thread->reloadGameSettings();
	}

	bool cardExists(const std::string& name)
	{
		return FileSystem::FileExists(PopnCard::GetCardPath(name).c_str());
	}

	bool isValidCardName(const std::string& name)
	{
		return !name.empty() && Path::SanitizeFileName(name) == name;
	}

	std::optional<std::string> askForNewName(QWidget* parent, const QString& title, const QString& current)
	{
		bool ok = false;
		const std::string name = QInputDialog::getText(parent, title, tr("Name:"), QLineEdit::Normal, current, &ok).trimmed().toStdString();
		if (!ok || name == current.toStdString())
			return std::nullopt;

		if (!isValidCardName(name) || cardExists(name))
		{
			QMessageBox::warning(parent, title, tr("Pick another name."));
			return std::nullopt;
		}
		return name;
	}
} // namespace

PopnCardManagerDialog::PopnCardManagerDialog(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Pop'n Card Manager"));
	resize(900, 580);

	m_list = new QListWidget(this);
	m_list->setIconSize(LIST_FACE_SIZE);
	m_list->setSpacing(2);
	m_list->setMinimumWidth(280);

	QPushButton* new_button = new QPushButton(tr("New..."), this);
	QPushButton* import_button = new QPushButton(tr("Import..."), this);
	QPushButton* folder_button = new QPushButton(tr("Open Folder"), this);
	QHBoxLayout* list_buttons = new QHBoxLayout();
	list_buttons->addWidget(new_button);
	list_buttons->addWidget(import_button);
	list_buttons->addWidget(folder_button);

	QVBoxLayout* list_layout = new QVBoxLayout();
	list_layout->addWidget(m_list);
	list_layout->addLayout(list_buttons);

	QWidget* empty_page = new QWidget(this);
	QVBoxLayout* empty_layout = new QVBoxLayout(empty_page);
	empty_layout->addStretch();
	QLabel* empty_label = new QLabel(tr("No cards yet."), empty_page);
	empty_label->setAlignment(Qt::AlignCenter);
	empty_layout->addWidget(empty_label);
	empty_layout->addStretch();

	QWidget* card_page = new QWidget(this);
	m_face = new QLabel(card_page);
	m_face->setFixedSize(DETAIL_FACE_SIZE);
	m_title = new QLabel(card_page);
	QFont title_font = m_title->font();
	title_font.setPointSizeF(title_font.pointSizeF() * 1.4);
	title_font.setBold(true);
	m_title->setFont(title_font);

	m_game = new QLabel(card_page);
	m_state = new QLabel(card_page);
	m_repair = new QPushButton(tr("Repair"), card_page);
	QHBoxLayout* state_layout = new QHBoxLayout();
	state_layout->addWidget(m_state);
	state_layout->addWidget(m_repair);
	state_layout->addStretch();
	m_number = new QLabel(card_page);
	m_number->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_card_data = new QComboBox(card_page);
	m_card_data_value = new QLabel(card_page);
	QHBoxLayout* data_layout = new QHBoxLayout();
	data_layout->addWidget(m_card_data);
	data_layout->addWidget(m_card_data_value);
	data_layout->addStretch();
	m_readers = new QLabel(card_page);
	m_readers->setWordWrap(true);
	m_twins = new QLabel(card_page);
	m_twins->setWordWrap(true);
	m_changed = new QLabel(card_page);

	QFormLayout* facts = new QFormLayout();
	facts->addRow(tr("Game:"), m_game);
	facts->addRow(tr("State:"), state_layout);
	facts->addRow(tr("Number:"), m_number);
	facts->addRow(tr("Card data:"), data_layout);
	facts->addRow(tr("In reader of:"), m_readers);
	facts->addRow(tr("Same number:"), m_twins);
	facts->addRow(tr("Last changed:"), m_changed);

	QVBoxLayout* summary_layout = new QVBoxLayout();
	summary_layout->addWidget(m_title);
	summary_layout->addLayout(facts);
	summary_layout->addStretch();

	QHBoxLayout* top_layout = new QHBoxLayout();
	top_layout->addWidget(m_face, 0, Qt::AlignTop);
	top_layout->addSpacing(12);
	top_layout->addLayout(summary_layout, 1);

	QPushButton* rename_button = new QPushButton(tr("Rename..."), card_page);
	m_copy = new QPushButton(card_page);
	QPushButton* export_button = new QPushButton(tr("Export..."), card_page);
	QPushButton* delete_button = new QPushButton(tr("Delete"), card_page);
	QHBoxLayout* card_buttons = new QHBoxLayout();
	card_buttons->addWidget(rename_button);
	card_buttons->addWidget(m_copy);
	card_buttons->addWidget(export_button);
	card_buttons->addWidget(delete_button);
	card_buttons->addStretch();

	const int byte_rows = (PopnCard::RECORD_SIZE + BYTE_COLUMNS - 1) / BYTE_COLUMNS;
	m_bytes = new QTableWidget(byte_rows, BYTE_COLUMNS, card_page);
	m_bytes->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_bytes->setSelectionMode(QAbstractItemView::NoSelection);
	m_bytes->setFocusPolicy(Qt::NoFocus);
	m_bytes->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	m_bytes->horizontalHeader()->setDefaultSectionSize(BYTE_COLUMN_WIDTH);
	m_bytes->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	QStringList column_labels;
	for (int column = 0; column < BYTE_COLUMNS; column++)
		column_labels.append(QStringLiteral("%1").arg(column, 0, 16).toUpper());
	QStringList row_labels;
	for (int row = 0; row < byte_rows; row++)
		row_labels.append(QStringLiteral("%1").arg(row * BYTE_COLUMNS, 2, 16, QLatin1Char('0')).toUpper());
	m_bytes->setHorizontalHeaderLabels(column_labels);
	m_bytes->setVerticalHeaderLabels(row_labels);
	QFont byte_font(QStringLiteral("monospace"));
	byte_font.setStyleHint(QFont::Monospace);
	m_bytes->setFont(byte_font);
	m_bytes->setFixedHeight(m_bytes->horizontalHeader()->sizeHint().height() + byte_rows * m_bytes->verticalHeader()->defaultSectionSize() + 4);

	QHBoxLayout* legend = new QHBoxLayout();
	legend->addWidget(new QLabel(tr("On the card:"), card_page));
	for (size_t i = 0; i < FIELDS.size() - 1; i++)
	{
		QLabel* entry = new QLabel(QStringLiteral("<span style=\"color:%1\">&#9632;</span> %2").arg(FIELDS[i].color.name()).arg(tr(FIELDS[i].name)), card_page);
		legend->addWidget(entry);
	}
	legend->addStretch();

	QVBoxLayout* card_layout = new QVBoxLayout(card_page);
	card_layout->addLayout(top_layout);
	card_layout->addLayout(card_buttons);
	card_layout->addSpacing(8);
	card_layout->addLayout(legend);
	card_layout->addWidget(m_bytes);
	card_layout->addStretch();

	m_pages = new QStackedWidget(this);
	m_pages->addWidget(empty_page);
	m_pages->addWidget(card_page);

	QHBoxLayout* body = new QHBoxLayout();
	body->addLayout(list_layout);
	body->addWidget(m_pages, 1);

	QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	QVBoxLayout* root = new QVBoxLayout(this);
	root->addLayout(body);
	root->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_list, &QListWidget::currentRowChanged, this, &PopnCardManagerDialog::showSelectedCard);
	connect(new_button, &QPushButton::clicked, this, &PopnCardManagerDialog::onNewClicked);
	connect(import_button, &QPushButton::clicked, this, &PopnCardManagerDialog::onImportClicked);
	connect(folder_button, &QPushButton::clicked, this, &PopnCardManagerDialog::onOpenFolderClicked);
	connect(rename_button, &QPushButton::clicked, this, &PopnCardManagerDialog::onRenameClicked);
	connect(m_copy, &QPushButton::clicked, this, &PopnCardManagerDialog::onCopyClicked);
	connect(export_button, &QPushButton::clicked, this, &PopnCardManagerDialog::onExportClicked);
	connect(delete_button, &QPushButton::clicked, this, &PopnCardManagerDialog::onDeleteClicked);
	connect(m_repair, &QPushButton::clicked, this, &PopnCardManagerDialog::onRepairClicked);
	connect(m_card_data, &QComboBox::currentIndexChanged, this, &PopnCardManagerDialog::onCardDataChanged);

	refreshList({});
}

PopnCardManagerDialog::~PopnCardManagerDialog() = default;

void PopnCardManagerDialog::migrateLegacySettings()
{
	for (const GameCardSetting& setting : readGameCardSettings())
	{
		INISettingsInterface ini(setting.ini_path);
		if (ini.Load() && PopnCard::AdoptLegacySettings(ini, setting.serial))
			ini.Save();
	}
}

QString PopnCardManagerDialog::describeCard(const std::string& name)
{
	const std::optional<PopnCard::Bytes> card = PopnCard::Load(PopnCard::GetCardPath(name));
	if (!card.has_value())
		return tr("Card not found");

	const PopnCard::Info info = PopnCard::Describe(card.value());
	QString text = QStringLiteral("%1 · %2").arg(gameName(info.game)).arg(stateName(info));
	if (!isBlank(info.type))
		text += QStringLiteral(" · %1").arg(groupNumber(info.number));
	return text;
}

void PopnCardManagerDialog::refreshList(const std::string& select_name)
{
	const std::vector<std::string> names = PopnCard::ListCards();
	const qreal dpr = devicePixelRatioF();

	{
		QSignalBlocker blocker(m_list);
		m_list->clear();
		for (const std::string& name : names)
		{
			const std::optional<PopnCard::Bytes> card = PopnCard::Load(PopnCard::GetCardPath(name));
			if (!card.has_value())
				continue;

			const PopnCard::Info info = PopnCard::Describe(card.value());
			const QString qname = QString::fromStdString(name);
			QListWidgetItem* item = new QListWidgetItem(QIcon(renderFace(info, qname, LIST_FACE_SIZE, dpr, false)),
				QStringLiteral("%1\n%2 · %3").arg(qname).arg(gameName(info.game)).arg(stateName(info)), m_list);
			item->setData(Qt::UserRole, qname);
		}
	}

	int row = 0;
	for (int i = 0; i < m_list->count(); i++)
	{
		if (m_list->item(i)->data(Qt::UserRole).toString().toStdString() == select_name)
			row = i;
	}

	m_pages->setCurrentIndex(m_list->count() == 0 ? 0 : 1);
	if (m_list->count() != 0)
		m_list->setCurrentRow(row);
	showSelectedCard();
}

std::optional<std::string> PopnCardManagerDialog::selectedName() const
{
	const QListWidgetItem* item = m_list->currentItem();
	if (!item)
		return std::nullopt;
	return item->data(Qt::UserRole).toString().toStdString();
}

void PopnCardManagerDialog::saveSelected(const PopnCard::Bytes& card)
{
	const std::optional<std::string> name = selectedName();
	if (!name.has_value())
		return;

	if (!PopnCard::Save(PopnCard::GetCardPath(name.value()), card))
		QMessageBox::critical(this, windowTitle(), tr("Could not save the card."));
	refreshList(name.value());
}

void PopnCardManagerDialog::showSelectedCard()
{
	const std::optional<std::string> name = selectedName();
	const std::optional<PopnCard::Bytes> card = name.has_value() ? PopnCard::Load(PopnCard::GetCardPath(name.value())) : std::nullopt;
	if (!card.has_value())
	{
		m_pages->setCurrentIndex(0);
		return;
	}

	const PopnCard::Info info = PopnCard::Describe(card.value());
	const QString qname = QString::fromStdString(name.value());
	m_face->setPixmap(renderFace(info, qname, DETAIL_FACE_SIZE, devicePixelRatioF(), true));
	m_title->setText(qname);
	m_game->setText(gameName(info.game));
	m_state->setText(stateName(info));
	m_repair->setVisible(isDamaged(info));
	m_number->setText(isBlank(info.type) ? tr("None yet") : groupNumber(info.number));

	const bool bonus_card = info.game == PopnCard::Game::Popn9 || info.game == PopnCard::Game::Popn10;
	m_card_data->setVisible(bonus_card);
	m_card_data_value->setVisible(!bonus_card);
	m_card_data_value->setText(QString::number(info.card_data));
	{
		QSignalBlocker blocker(m_card_data);
		fillCardDataChoices(m_card_data, info.game);
		const int index = m_card_data->findData(static_cast<int>(info.card_data));
		m_card_data->setCurrentIndex(index);
		m_card_data_value->setVisible(!bonus_card || index < 0);
	}

	QStringList readers;
	for (const GameCardSetting& setting : readGameCardSettings())
	{
		if (setting.card_name == name.value())
			readers.append(gameTitle(setting));
	}
	m_readers->setText(readers.isEmpty() ? tr("No game") : readers.join(QStringLiteral(", ")));

	QStringList twins;
	if (!isBlank(info.type))
	{
		for (const std::string& other : PopnCard::ListCards())
		{
			if (other == name.value())
				continue;
			const std::optional<PopnCard::Bytes> other_card = PopnCard::Load(PopnCard::GetCardPath(other));
			if (other_card.has_value() && PopnCard::Describe(other_card.value()).number == info.number)
				twins.append(QString::fromStdString(other));
		}
	}
	m_twins->setText(twins.isEmpty() ? tr("None") : twins.join(QStringLiteral(", ")));
	m_changed->setText(QLocale().toString(QFileInfo(QString::fromStdString(PopnCard::GetCardPath(name.value()))).lastModified(), QLocale::ShortFormat));

	const bool copyable = info.game == PopnCard::Game::Popn9 || info.game == PopnCard::Game::Popn10;
	m_copy->setVisible(copyable && (isRegistered(info.type) || isBlank(info.type)));
	m_copy->setText(info.game == PopnCard::Game::Popn9 ? tr("Copy for pop'n 10") : tr("Copy for pop'n 9"));

	for (int i = 0; i < m_bytes->rowCount() * BYTE_COLUMNS; i++)
	{
		const u32 offset = static_cast<u32>(i);
		QTableWidgetItem* item = new QTableWidgetItem();
		item->setTextAlignment(Qt::AlignCenter);
		if (offset < PopnCard::RECORD_SIZE)
		{
			const auto field = std::find_if(FIELDS.begin(), FIELDS.end(), [offset](const Field& f) { return offset >= f.from && offset <= f.to; });
			QColor background = field->color;
			background.setAlpha(FIELD_ALPHA);
			item->setText(QStringLiteral("%1").arg(static_cast<int>(card.value()[offset]), 2, 16, QLatin1Char('0')).toUpper());
			item->setBackground(background);
			item->setToolTip(tr(field->name));
		}
		m_bytes->setItem(i / BYTE_COLUMNS, i % BYTE_COLUMNS, item);
	}

	m_pages->setCurrentIndex(1);
}

void PopnCardManagerDialog::onNewClicked()
{
	QDialog dialog(this);
	dialog.setWindowTitle(tr("New Card"));

	QLineEdit* name = new QLineEdit(&dialog);
	QComboBox* game = new QComboBox(&dialog);
	game->addItem(gameName(PopnCard::Game::Popn9), static_cast<int>(PopnCard::Game::Popn9));
	game->addItem(gameName(PopnCard::Game::Popn10), static_cast<int>(PopnCard::Game::Popn10));
	QLineEdit* number = new QLineEdit(&dialog);
	number->setPlaceholderText(tr("16 hex digits"));
	number->setMinimumWidth(number->fontMetrics().horizontalAdvance(groupNumber(BLANK_NUMBER)) + NUMBER_FIELD_PADDING);
	QPushButton* random = new QPushButton(tr("Random"), &dialog);
	QHBoxLayout* number_layout = new QHBoxLayout();
	number_layout->addWidget(number, 1);
	number_layout->addWidget(random);
	QCheckBox* blank = new QCheckBox(tr("Blank card"), &dialog);
	QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

	QFormLayout* layout = new QFormLayout(&dialog);
	layout->addRow(tr("Name:"), name);
	layout->addRow(tr("Game:"), game);
	layout->addRow(tr("Number:"), number_layout);
	layout->addRow(QString(), blank);
	layout->addRow(buttons);
	dialog.resize(NEW_CARD_DIALOG_WIDTH, dialog.sizeHint().height());

	const std::optional<std::string> selected = selectedName();
	const std::optional<PopnCard::Bytes> selected_card = selected.has_value() ? PopnCard::Load(PopnCard::GetCardPath(selected.value())) : std::nullopt;
	if (selected_card.has_value())
	{
		const PopnCard::Info info = PopnCard::Describe(selected_card.value());
		if (isRegistered(info.type))
			number->setText(groupNumber(info.number));
	}

	connect(random, &QPushButton::clicked, &dialog, [number]() { number->setText(groupNumber(PopnCard::RandomNumber())); });
	connect(blank, &QCheckBox::toggled, &dialog, [number, random](bool checked) {
		number->setEnabled(!checked);
		random->setEnabled(!checked);
	});
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	while (dialog.exec() == QDialog::Accepted)
	{
		const std::string card_name = name->text().trimmed().toStdString();
		if (!isValidCardName(card_name) || cardExists(card_name))
		{
			QMessageBox::warning(&dialog, dialog.windowTitle(), tr("Pick another name."));
			continue;
		}

		const std::optional<std::string> card_number = blank->isChecked() ? std::optional<std::string>(BLANK_NUMBER) :
		                                                                     PopnCard::NormalizeNumber(number->text().toStdString());
		if (!card_number.has_value())
		{
			QMessageBox::warning(&dialog, dialog.windowTitle(), tr("The number needs 16 hex digits."));
			continue;
		}

		const bool popn10 = static_cast<PopnCard::Game>(game->currentData().toInt()) == PopnCard::Game::Popn10;
		const PopnCard::Type type = blank->isChecked() ? (popn10 ? PopnCard::Type::Popn10Blank : PopnCard::Type::Popn9Blank) :
		                                                 (popn10 ? PopnCard::Type::Popn10Registered : PopnCard::Type::Popn9Registered);
		if (!PopnCard::Save(PopnCard::GetCardPath(card_name), PopnCard::Build(type, 0, card_number.value())))
		{
			QMessageBox::critical(this, windowTitle(), tr("Could not save the card."));
			return;
		}

		refreshList(card_name);
		return;
	}
}

void PopnCardManagerDialog::onImportClicked()
{
	const QString path = QFileDialog::getOpenFileName(this, tr("Import Card"), QString(), tr("Card Files (*.bin);;All Files (*)"));
	if (path.isEmpty())
		return;

	const std::optional<PopnCard::Bytes> card = PopnCard::Load(path.toStdString());
	if (!card.has_value())
	{
		QMessageBox::warning(this, windowTitle(), tr("That is not a card file."));
		return;
	}

	const std::string name = PopnCard::UniqueName(QFileInfo(path).completeBaseName().toStdString());
	if (!PopnCard::Save(PopnCard::GetCardPath(name), card.value()))
	{
		QMessageBox::critical(this, windowTitle(), tr("Could not save the card."));
		return;
	}
	refreshList(name);
}

void PopnCardManagerDialog::onOpenFolderClicked()
{
	const std::string directory = PopnCard::GetWalletDirectory();
	FileSystem::EnsureDirectoryExists(directory.c_str(), true);
	QtUtils::OpenURL(this, QUrl::fromLocalFile(QString::fromStdString(directory)));
}

void PopnCardManagerDialog::onRenameClicked()
{
	const std::optional<std::string> name = selectedName();
	if (!name.has_value())
		return;

	const std::optional<std::string> new_name = askForNewName(this, tr("Rename Card"), QString::fromStdString(name.value()));
	if (!new_name.has_value())
		return;

	if (!FileSystem::RenamePath(PopnCard::GetCardPath(name.value()).c_str(), PopnCard::GetCardPath(new_name.value()).c_str()))
	{
		QMessageBox::critical(this, windowTitle(), tr("Could not rename the card."));
		return;
	}

	replaceCardInGameSettings(name.value(), new_name.value());
	refreshList(new_name.value());
}

void PopnCardManagerDialog::onCopyClicked()
{
	const std::optional<std::string> name = selectedName();
	const std::optional<PopnCard::Bytes> card = name.has_value() ? PopnCard::Load(PopnCard::GetCardPath(name.value())) : std::nullopt;
	if (!card.has_value())
		return;

	const PopnCard::Info info = PopnCard::Describe(card.value());
	const bool to_popn10 = info.game == PopnCard::Game::Popn9;
	const PopnCard::Type type = isBlank(info.type) ? (to_popn10 ? PopnCard::Type::Popn10Blank : PopnCard::Type::Popn9Blank) :
	                                                 (to_popn10 ? PopnCard::Type::Popn10Registered : PopnCard::Type::Popn9Registered);
	const std::string copy_name = PopnCard::UniqueName(
		QStringLiteral("%1 (%2)").arg(QString::fromStdString(name.value())).arg(gameName(to_popn10 ? PopnCard::Game::Popn10 : PopnCard::Game::Popn9)).toStdString());
	if (!PopnCard::Save(PopnCard::GetCardPath(copy_name), PopnCard::Build(type, info.card_data, info.number)))
	{
		QMessageBox::critical(this, windowTitle(), tr("Could not save the card."));
		return;
	}
	refreshList(copy_name);
}

void PopnCardManagerDialog::onExportClicked()
{
	const std::optional<std::string> name = selectedName();
	const std::optional<PopnCard::Bytes> card = name.has_value() ? PopnCard::Load(PopnCard::GetCardPath(name.value())) : std::nullopt;
	if (!card.has_value())
		return;

	const QString path = QFileDialog::getSaveFileName(this, tr("Export Card"), QString::fromStdString(name.value() + ".bin"),
		tr("Card Files (*.bin);;All Files (*)"));
	if (path.isEmpty())
		return;

	if (!PopnCard::Save(path.toStdString(), card.value()))
		QMessageBox::critical(this, windowTitle(), tr("Could not save the card."));
}

void PopnCardManagerDialog::onDeleteClicked()
{
	const std::optional<std::string> name = selectedName();
	if (!name.has_value())
		return;

	if (QMessageBox::question(this, tr("Delete Card"), tr("Delete \"%1\"?").arg(QString::fromStdString(name.value()))) != QMessageBox::Yes)
		return;

	if (!FileSystem::DeleteFilePath(PopnCard::GetCardPath(name.value()).c_str()))
	{
		QMessageBox::critical(this, windowTitle(), tr("Could not delete the card."));
		return;
	}

	replaceCardInGameSettings(name.value(), {});
	refreshList({});
}

void PopnCardManagerDialog::onRepairClicked()
{
	const std::optional<std::string> name = selectedName();
	std::optional<PopnCard::Bytes> card = name.has_value() ? PopnCard::Load(PopnCard::GetCardPath(name.value())) : std::nullopt;
	if (!card.has_value())
		return;

	PopnCard::RepairChecksums(card.value());
	saveSelected(card.value());
}

void PopnCardManagerDialog::onCardDataChanged(int index)
{
	const std::optional<std::string> name = selectedName();
	std::optional<PopnCard::Bytes> card = name.has_value() ? PopnCard::Load(PopnCard::GetCardPath(name.value())) : std::nullopt;
	if (!card.has_value() || index < 0)
		return;

	PopnCard::SetCardData(card.value(), static_cast<u8>(m_card_data->itemData(index).toInt()));
	saveSelected(card.value());
}

#include "moc_PopnCardManagerDialog.cpp"
