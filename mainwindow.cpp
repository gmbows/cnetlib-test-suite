#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <cnetlib4/cnetlib4.h>
#include <gcutils/gcutils.h>
#include <libgen.h>
#include <QTimer>
#include <QFileSystemModel>

struct test {
	int a,b,c;
	const char* s;
};

enum UserMessageType : int32_t {
	REQ_DIR_LISTING = 90,
	DIR_LISTING = 91,
	REQ_FILE = 92,
	REQ_CHANNEL_SIZE = 93,
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);

	gcutils::log_handler = [this](std::string s) {
		emit(this->sendLog(s));
	};

	connect(this,&MainWindow::sendLog,this,&MainWindow::appendToLog);

	float rate = 1000/30.f;
	QTimer *throughput_timer = new QTimer();
	connect(throughput_timer, &QTimer::timeout, [this,rate]() {
		if(this->serv.connections.empty()) return;
		float tput = this->serv.connections.at(0)->net_reader.stats.check()/1000000*rate;
		std::string label = std::to_string(tput)+"Mb/s";
		this->ui->tputLabel->setText(QString::fromStdString(label));
	});
	throughput_timer->start(rate);

	this->serv.setChannelHandler([this](cn::CommunicationChannel* cc) {
		QVariant v = QVariant();
		v.setValue(cc);
		QListWidgetItem *i = new QListWidgetItem(this->ui->listWidget);
		i->setData(0,v);
		i->setText(QString::fromStdString(cc->control_address()));
		this->ui->listWidget->addItem(i);
	});

	this->serv.setMessageHandler(REQ_CHANNEL_SIZE,[](cn::Connection *c,cn::NetworkMessage &msg) {
		CHAN_IDTYPE_T identifier;
		msg >> identifier;
		auto chan = c->owner->find_channel(identifier);
		int size = chan->size();
		auto resp = cn::NetworkMessage(234,msg);
		resp << size;
		c->send(resp);
	});

	this->serv.setMessageHandler(REQ_DIR_LISTING,[](cn::Connection *c,cn::NetworkMessage &msg){
		std::string share_dir = "./sharing";
		auto response = cn::NetworkMessage(DIR_LISTING,msg);
		auto listing = gcutils::dir_listing_str(share_dir);
		std::vector<std::string> listing_trimmed;
		for(auto &p : listing) {
			auto sp = gcutils::make_relative_to(p,"sharing",false);
			gcutils::sanitize_path(sp);
			listing_trimmed.push_back(sp.string());
		}
		response << listing_trimmed;
		c->send(response);
	});

	this->serv.setMessageHandler(REQ_FILE,[this](cn::Connection *c,cn::NetworkMessage &msg){
		std::string path;
		msg >> path;
		path = "./sharing/"+path;
		gcutils::log(c->getInfo(),": Got request for file ",path);
		std::string fn = gcutils::get_filename(path);
		c->send_file_with_name_async("./sharing/"+this->selectedFile(),"received/"+gcutils::make_relative_to(path,"./sharing",false).string());
	});

	connect(this->ui->startServer,&QPushButton::clicked,[this](){this->serv.start();this->ui->serverStatus->setText("Running");});
	connect(this->ui->stopServer,&QPushButton::clicked,[this](){this->serv.stop();this->ui->serverStatus->setText("Stopped");});
	connect(this->ui->connectButton,&QPushButton::clicked,[this](){
		this->client.open(this->ui->address->text().toStdString());
	});
	connect(this->ui->treeView,&QTreeView::clicked,[this](){
//		gcutils::print(this->client.get_channel(0)->size());
		std::string fn = gcutils::get_filename(this->selectedFile());
		this->ui->selectedFile->setText(QString::fromStdString(fn));
	});
	connect(this->ui->sendMessage,&QPushButton::clicked,[this]() {
		auto stuff = cn::NetworkMessage(REQ_DIR_LISTING);
		this->client.get_channel(0)->get()->send(stuff,[this](cn::NetworkMessage &response) {
			std::vector<std::string> files;
			response >> files;
			emit(this->needsUpdateTree(files));
		});
//		gcutils::print("Got directory listing: ",listing.getSize());
//		auto msg = cn::NetworkMessage(REQ_CHANNEL_SIZE);
//		msg << this->client.get_channel(0)->identifier;
//		auto response = this->client.get_channel(0)->get()->send_for_response(msg);
//		gcutils::print("Got response: ",response.getSize());
//		int size;
//		response >> size;
//		gcutils::print(size);
	});
	connect(this->ui->requestFile,&QPushButton::clicked,[this]() {
		auto stuff = cn::NetworkMessage(REQ_FILE);
		stuff << this->selectedFile();
		this->client.connections.at(0)->send(stuff);
	});
	connect(this,&MainWindow::needsUpdateTree,this,&MainWindow::updateTree);
	connect(this->ui->sendAll,&QPushButton::clicked,[this]() {
		std::string path = this->ui->path->text().toStdString();
		auto files = gcutils::dir_listing(path);
		for(auto &f : files) {
			this->client.get_channel(0)->get()->send_file_with_name_async(f.string(),"received/"+gcutils::make_relative_to(f,path).string());
		}
	});

}

MainWindow::~MainWindow() {
	delete ui;
}

