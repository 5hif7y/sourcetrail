#include "QtLicenseWindow.h"

#include "licenses.h"
#include "QtExpanderButton.h"

#include <aidkit/qt/Strings.hpp>

#include <QLabel>
#include <QVBoxLayout>
#include <QToolButton>

using namespace std;
using namespace aidkit::qt;

constexpr int WIDTH = 550;
constexpr int SPACING = 20;

static QtExpanderButton *createNameButton(const QString &name, const QString &version)
{
	QString buttonText = name;
	if (!version.isEmpty())
		buttonText += " (v%1)"_qs.arg(version);

	QtExpanderButton *button = new QtExpanderButton(buttonText);

	QFont font = button->font();
	font.setPixelSize(36);
	font.setBold(true);
	button->setFont(font);

	return button;
}

static QLabel *createUrlLabel(const QString &url)
{
	QLabel *label = new QLabel("<a href=\"%1\">%1</a>"_qs.arg(url));
	label->setOpenExternalLinks(true);

	return label;
}

static QLabel *createTextLabel(const QString &text)
{
	QLabel *label = new QLabel(text);
	label->setFixedWidth(WIDTH);
	label->setWordWrap(true);

	return label;
}

static void addLicense(QWidget *parent, QBoxLayout *layout, const LicenseInfo &licenseInfo)
{
	QtExpanderButton *nameButton = createNameButton(licenseInfo.name, licenseInfo.version);
	QLabel *urlLabel = createUrlLabel(licenseInfo.url);
	QLabel *textLabel = createTextLabel(licenseInfo.license);
	textLabel->setVisible(false);

	QtExpanderButton::connect(nameButton, &QtExpanderButton::expanded, parent, [=](bool expanded)
	{
		textLabel->setVisible(expanded);
	});

	layout->addWidget(nameButton);
	layout->addWidget(urlLabel);
	layout->addWidget(textLabel);

	layout->addSpacing(SPACING);
}

QtLicenseWindow::QtLicenseWindow(QWidget* parent)
	: QtWindow(false, parent)
{
	setScrollAble(true);
}

QSize QtLicenseWindow::sizeHint() const
{
	return QSize(650, 600);
}

void QtLicenseWindow::populateWindow(QWidget* widget)
{
	QVBoxLayout* layout = new QVBoxLayout(widget);

	addLicense(this, layout, licenseApp);

	layout->addWidget(createTextLabel(tr("<b>Copyrights and Licenses for Third Party Software Distributed with Sourcetrail:</b>")));
	layout->addSpacing(SPACING);

	for (LicenseInfo license: licenses3rdParties)
		addLicense(this, layout, license);

	widget->setLayout(layout);
}

void QtLicenseWindow::windowReady()
{
	updateTitle(tr("License"));
	updateCloseButton(tr("Close"));

	setNextVisible(false);
	setPreviousVisible(false);
}
