#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <cnetlib4/cnetlib4.h>
#include "ui_mainwindow.h"
#include "virtualfilesystemmodel.h"
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

	cn::Server serv = cn::Server(5555);
	cn::Client client = cn::Client(5555);

	std::string selectedFile() {
		auto m_index = this->ui->treeView->currentIndex();
		auto data = this->ui->treeView->model()->data(m_index);
		return data.toString().toStdString();
	}

private slots:
	void updateTree(std::vector<std::string> files) {
		TreeModel *model = new TreeModel({"Path"},files);
//		auto data = model->index(0,0,QModelIndex()).data();
		this->ui->treeView->setModel(model);
	}
	void appendToLog(const std::string &str) {
		std::string lsnl = "<b>"+ gcutils::get_timestamp()+"</b>: "+str+"\n";
		this->ui->log->append(QString::fromStdString(lsnl));
	}

Q_SIGNALS:
	void needsUpdateTree(std::vector<std::string>);
	void sendLog(const std::string &str);

private:
	Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
