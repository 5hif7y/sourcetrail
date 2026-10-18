#ifndef QT_FILES_AND_DIRECTORIES_DIALOG
#define QT_FILES_AND_DIRECTORIES_DIALOG

#include <QFileDialog>

class QListView;

class QtFilesAndDirectoriesDialog: public QFileDialog
{
	Q_OBJECT
public:
	QtFilesAndDirectoriesDialog(QWidget* parent);
	~QtFilesAndDirectoriesDialog() override;

public slots:
	void chooseClicked();

private:
	bool eventFilter(QObject* obj, QEvent* event) override;
};

#endif	  // QT_FILES_AND_DIRECTORIES_DIALOG
