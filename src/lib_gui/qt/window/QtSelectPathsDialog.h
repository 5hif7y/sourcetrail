#ifndef QT_SELECT_PATHS_DIALOG_H
#define QT_SELECT_PATHS_DIALOG_H

#include "QtTextEditDialog.h"

class FilePath;
class QListWidget;

class QtSelectPathsDialog: public QtTextEditDialog
{
	Q_OBJECT
public:
	QtSelectPathsDialog(const QString& title, const QString& description, QWidget* parent = nullptr);

	std::vector<FilePath> getPathsList() const;
	void setPathsList(
		const std::vector<FilePath>& paths,
		const std::vector<FilePath>& checkedPaths,
		const FilePath& rootPathForRelativePaths);

	void populateWindow(QWidget* widget) override;
	void windowReady() override;

private:
	QListWidget* m_list;

	void checkSelected(bool checked);
};

#endif	  // QT_SELECT_PATHS_DIALOG_H
