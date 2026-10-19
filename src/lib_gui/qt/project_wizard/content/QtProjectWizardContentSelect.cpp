#include "QtProjectWizardContentSelect.h"

#include "LanguageType.h"
#include "QtFlowLayout.h"
#include "QtMessageBox.h"
#include "QtProjectWizardWindow.h"
#include "SourceGroupUiRegistry.h"
#include "UiPlugin.h"
#include "utilityQt.h"
#include "utilityString.h"

#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>

#include <regex>

using namespace utility;

QtProjectWizardContentSelect::QtProjectWizardContentSelect(QtProjectWizardWindow* window)
	: QtProjectWizardContent(window)
{
}

void QtProjectWizardContentSelect::populate(QGridLayout* layout, int&  /*row*/)
{
	st::plugin::ensureUiPluginLoaded();

	// The set is owned by the UI plugin; the wizard only renders it.
	std::map<LanguageType, std::vector<SourceGroupUiEntry>> sourceGroupInfos;
	std::map<SourceGroupType, std::string> sourceGroupDescriptions;
	for (const SourceGroupUiEntry& entry: SourceGroupUiRegistry::instance().entries())
	{
		sourceGroupInfos[entry.language].push_back(entry);
		sourceGroupDescriptions[entry.type] = entry.description;
	}

	QVBoxLayout* vlayout = new QVBoxLayout();
	vlayout->setContentsMargins(0, 10, 0, 0);
	vlayout->setSpacing(10);

	m_languages = new QButtonGroup();
	for (auto& it: sourceGroupInfos)
	{
		QPushButton* b = new QPushButton(languageTypeToString(it.first).c_str(), this);
		b->setObjectName(QStringLiteral("menuButton"));
		b->setCheckable(true);
		b->setProperty("language_type", QVariant::fromValue(it.first));
		m_languages->addButton(b);
		vlayout->addWidget(b);
	}

	vlayout->addStretch();
	layout->addLayout(vlayout, 0, QtProjectWizardWindow::FRONT_COL, Qt::AlignRight);

	connect(m_languages, qOverload<QAbstractButton*>(&QButtonGroup::buttonClicked), [this](QAbstractButton* button)
	{
		LanguageType selectedLanguage = qt_variant_cast<LanguageType>(button->property("language_type"));

		bool hasRecommended = false;
		for (auto& it: m_buttons)
		{
			it.second->setExclusive(false);
			for (QAbstractButton* button: it.second->buttons())
			{
				button->setChecked(false);
				button->setVisible(it.first == selectedLanguage);

				if (it.first == selectedLanguage)
				{
					hasRecommended = hasRecommended | button->property("recommended").toBool();
				}
			}
			it.second->setExclusive(true);
		}

		m_window->setNextEnabled(false);
		m_title->setText("Source Group Types - " + m_languages->checkedButton()->text());

		m_description->setText(
			hasRecommended ? QStringLiteral("<b>* recommended</b>") : QLatin1String(""));
	});

	QtFlowLayout* flayout = new QtFlowLayout(10, 0, 0);

	for (auto& languageIt: sourceGroupInfos)
	{
		QButtonGroup* sourceGroupButtons = new QButtonGroup(this);

		for (auto& sourceGroupIt: languageIt.second)
		{
			std::string name = sourceGroupTypeToProjectSetupString(sourceGroupIt.type);
			if (sourceGroupIt.recommended)
			{
				name += "*";
			}

			QToolButton* b = createSourceGroupButton(
				QString::fromStdString(utility::insertLineBreaksAtBlankSpaces(name, 15)),
				QString::fromStdString(sourceGroupIt.iconName));

			if (sourceGroupIt.recommended)
			{
				b->setStyleSheet(QStringLiteral("font-weight: bold"));
			}

			b->setProperty("source_group_type", QVariant::fromValue(sourceGroupIt.type));
			b->setProperty("recommended", sourceGroupIt.recommended);
			sourceGroupButtons->addButton(b);
			flayout->addWidget(b);
		}

		m_buttons[languageIt.first] = sourceGroupButtons;
	}

	for (auto& it: m_buttons)
	{
		connect(it.second, qOverload<QAbstractButton *>(&QButtonGroup::buttonClicked), [this, sourceGroupDescriptions](QAbstractButton *button)
		{
			SourceGroupType selectedType = qt_variant_cast<SourceGroupType>(button->property("source_group_type"));
			const auto description = sourceGroupDescriptions.find(selectedType);
			m_description->setText(QString::fromStdString(
				description != sourceGroupDescriptions.end() ? description->second : std::string()));

			m_window->setNextEnabled(true);
		});
	}

	QWidget* container = new QWidget();
	QVBoxLayout* containerLayout = new QVBoxLayout();
	containerLayout->setContentsMargins(0, 0, 0, 0);

	QFrame* groupContainer = new QFrame();
	groupContainer->setObjectName(QStringLiteral("sourceGroupContainer"));
	groupContainer->setLayout(flayout);
	containerLayout->addWidget(groupContainer, 0);

	m_description = new QLabel(QStringLiteral(" \n \n"));
	m_description->setWordWrap(true);
	m_description->setOpenExternalLinks(true);
	m_description->setObjectName(QStringLiteral("sourceGroupDescription"));
	m_description->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
	m_description->setMinimumHeight(80);
	containerLayout->addWidget(m_description, 0);

	container->setLayout(containerLayout);
	layout->addWidget(container, 0, QtProjectWizardWindow::BACK_COL);

	m_title = new QLabel(QStringLiteral("Source Group Types"));
	m_title->setObjectName(QStringLiteral("sourceGroupTitle"));

	layout->addWidget(m_title, 0, QtProjectWizardWindow::BACK_COL, Qt::AlignLeft | Qt::AlignTop);

	layout->setRowStretch(0, 0);
	layout->setColumnStretch(QtProjectWizardWindow::FRONT_COL, 0);
	layout->setColumnStretch(QtProjectWizardWindow::BACK_COL, 1);
	layout->setHorizontalSpacing(0);

	if (!m_languages->buttons().isEmpty())
	{
		m_languages->buttons().constFirst()->click();
	}
}

void QtProjectWizardContentSelect::save()
{
	SourceGroupType selectedType = SourceGroupType::UNKNOWN;

	for (auto& it: m_buttons)
	{
		if (QAbstractButton* b = it.second->checkedButton())
		{
			selectedType = qt_variant_cast<SourceGroupType>(b->property("source_group_type"));
			if (selectedType != SourceGroupType::UNKNOWN)
			{
				break;
			}
		}
	}
	emit selected(selectedType);
}

bool QtProjectWizardContentSelect::check()
{
	bool sourceGroupChosen = false;

	for (auto& it: m_buttons)
	{
		if (it.second->checkedId() != -1)
		{
			sourceGroupChosen = true;
			break;
		}
	}

	if (!sourceGroupChosen)
	{
		QtMessageBox msgBox(m_window);
		msgBox.setText(QStringLiteral("Please choose a method of creating a new Source Group."));
		msgBox.execModal();
		return false;
	}

	return true;
}
