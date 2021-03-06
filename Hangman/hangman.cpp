#include "hangman.h"
#include "ui_hangman.h"
#include <iostream>
#include <QMessageBox>

using namespace std;

Hangman::Hangman(QWidget *parent) : QMainWindow(parent), ui(new Ui::Hangman) {
    ui->setupUi(this);
    addButonsToArray();
    connect(ui->connectButton, &QPushButton::clicked, this, &Hangman::connectButtonHit);
    connect(ui->hostLineEdit, &QLineEdit::returnPressed, ui->connectButton, &QPushButton::click);
    connect(ui->readyButton, &QPushButton::clicked, this, &Hangman::readyButtonHit);
    for(int i = 0; i < 26 ; i++) {
        connect(letterButtons[i], &QPushButton::clicked, this, std::bind(&Hangman::letterButtonHit, this, i +'A'));
    }
}

Hangman::~Hangman() {
    delete ui;
}

void Hangman::connectButtonHit() {
    ui->connectGroup->setEnabled(false);

    auto host = ui->hostLineEdit->text();
    int port = ui->portSpinBox->text().toInt();

    sock = new QTcpSocket(this);
    sock->connectToHost(host, port);

    connect(sock, &QTcpSocket::connected, this, &Hangman::socketConnected);
    connect(sock, &QTcpSocket::readyRead, this, &Hangman::readData);

    connTimeoutTimer = new QTimer(this);
    connTimeoutTimer->setSingleShot(true);
    connect(connTimeoutTimer, &QTimer::timeout, [&]{
        connTimeoutTimer->deleteLater();
        ui->connectGroup->setEnabled(true);
        QMessageBox::critical(this, "Error", "Connect timed out");
     });
     connTimeoutTimer->start(3000);
}

void Hangman::socketConnected() {
    connTimeoutTimer->stop();
    connTimeoutTimer->deleteLater();
    ui->hangmanGroup->setEnabled(true);
    ui->wordTextEdit->setEnabled(false);
    ui->rankingTextEdit->setEnabled(false);
}

void Hangman::readData() {
    QByteArray dane = sock->read(512);
    qDebug()<<dane;
    if(dane[0] == GAME_STARTED) {
        getWordAndRanking(dane, started);
    } else if (dane[0] == GET_FD) {
        getFdAndRanking(dane);
    } else if (dane[0] == GET_RANKING) {
        getRanking(dane);
    } else if (dane[0] == GAME_ENDED) {
        gameEndedAndRanking(dane);
    } else if (dane[0] >= 'A' && dane[0] <= 'Z') {
        disableButton(dane);
    } else if (dane[0] == GAME_ALREADY_STARTED) {
        gameAlreadyStarted(dane);
    }
}

void Hangman::readyButtonHit() {
    sendData(READY);
}

void Hangman::sendData(char c) {
    QByteArray data(1,c);

    sock->write(data);
}

void Hangman::sendData(QString s) {
    auto data = (s).toUtf8();

    sock->write(data);
}

void Hangman::getWord(QByteArray dane) {
    array<int, 2> lengthAndShift = getMessageLengthAndShift(dane);
    int length = lengthAndShift[0];
    int shift = lengthAndShift[1];
    ui->lettersGroup->setEnabled(true);
    QByteArray word;
    for(int i = 0; i <= length; i++) {
        word[i] = dane[i+2+shift];
    }
    ui->wordTextEdit->append(QString::fromUtf8(word).trimmed());
    ui->wordTextEdit->setAlignment(Qt::AlignCenter);
    QFont font("Noto Sans", 16);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    ui->wordTextEdit->setFont(font);
    ui->readyButton->setEnabled(false);
}

void Hangman::getFd(QByteArray dane) {
    array<int, 2> lengthAndShift = getMessageLengthAndShift(dane);
    int length = lengthAndShift[0];
    int shift = lengthAndShift[1];

    QByteArray fd;
    for(int i = 0; i <= length; i++) {
        fd[i] = dane[i+2 + shift];
    }
    QString playerName = "PLAYER: " + QString::fromUtf8(fd).trimmed();
    ui->playerLabel->setText(playerName);
}

void Hangman::getRanking(QByteArray dane) {
    array<int, 2> lengthAndShift = getMessageLengthAndShift(dane);
    int length = lengthAndShift[0];
    int shift = lengthAndShift[1];

    QByteArray ranking;
    for(int i = 0; i< length; i++) {
        ranking[i] = dane[i+2+shift];
    }
    ui->rankingTextEdit->clear();
    ui->rankingTextEdit->append(QString::fromUtf8(ranking));

    if(dane[length + 2 +shift] == GAME_STARTED) {
        QByteArray word;
        for(int i = 0; i < dane.length(); i++) {
            word[i] = dane[i+length+2+shift];
        }
        getWord(word);
    }
}

void Hangman::getFdAndRanking(QByteArray dane) {
    playerLifes = MAX_LIFES;
    array<int, 2> lengthAndShift = getMessageLengthAndShift(dane);
    int length = lengthAndShift[0];
    int shift = lengthAndShift[1];

    QByteArray fd;
    for(int i = 0; i < length; i++) {
        fd[i] = dane[i+2 + shift];
    }
    QString playerName = "PLAYER: " + QString::fromUtf8(fd).trimmed();
    ui->playerLabel->setText(playerName);
    int lengthFd = length + 2 + shift;
    QByteArray ranking;
    for(int i = 0; i <= dane.length(); i++) {
        ranking[i] = dane[i + lengthFd];
    }
    getRanking(ranking);
}

void Hangman::getWordAndRanking(QByteArray dane, bool isStarted) {
    if(!isStarted) {
        started = true;
        enableButtons();
        ui->wordTextEdit->clear();
        ui->shareLifesLabel->setText(QString::number(MAX_LIFES));
        showPic();
        if(playerLifes == 0)
            playerLifes = MAX_LIFES;
        ui->yourLifesLabel->setText(QString::number(playerLifes));
    }
    array<int, 2> lengthAndShift = getMessageLengthAndShift(dane);
    int length = lengthAndShift[0];
    int shift = lengthAndShift[1];
    ui->lettersGroup->setEnabled(true);
    QByteArray word;
    for(int i = 0; i < length; i++) {
        word[i] = dane[i+2+shift];
    }
    ui->wordTextEdit->append(QString::fromUtf8(word).trimmed());
    ui->wordTextEdit->setAlignment(Qt::AlignCenter);
    QFont font("Noto Sans", 16);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    ui->wordTextEdit->setFont(font);
    ui->readyButton->setEnabled(false);
    int lengthWord = length +2 +shift;
    QByteArray ranking;
    for(int i = 0; i <= dane.length(); i++) {
        ranking[i] = dane[i + lengthWord];
    }
    getRanking(ranking);
    ui->readyButton->setEnabled(false);
}


void Hangman::gameEndedAndRanking(QByteArray dane) {
    ui->readyButton->setEnabled(true);
    ui->wordTextEdit->clear();
    int win = dane[1] - '0';
    if(win == 1)
        ui->wordTextEdit->setText("WIN");
    else {
        ui->wordTextEdit->setText("LOSS");
        ui->shareLifesLabel->setText("0");
        showPic();
    }

    QByteArray ranking;
    for(int i = 0; i <= dane.length(); i++) {
        ranking[i] = dane[i + 2];
    }
    getRanking(ranking);
    ui->hangmanGroup->setEnabled(true);
    ui->lettersGroup->setEnabled(false);
    started = false;
}

void Hangman::gameAlreadyStarted(QByteArray dane) {
    ui->readyButton->setEnabled(false);
    ui->wordTextEdit->setText("GAME ALREADY STARTED");
    QByteArray fd;
    for(int i = 0; i <= dane.length(); i++) {
        fd[i] = dane[i+1];
    }
    getFdAndRanking(fd);
}

array<int,2> Hangman::getMessageLengthAndShift(QByteArray dane) {
    array<int, 2> lengthAndShift;
    int i = 1;
    int j = 0;
    string lengthString;
    while(dane[i] != ';') {
        lengthString[j] = dane[i];
        i++;
        j++;
    }
    lengthAndShift[0] = stoi(lengthString);
    lengthAndShift[1] = j;
    return lengthAndShift;
}

void Hangman::disableButton(QByteArray dane) {
    if(started) {
        int i = 0;
        for(char x = 'A'; x <= 'Z'; x++) {
            if(x == dane[0]) {
                letterButtons[i]->setEnabled(false);
                qDebug()<<"poszedl disable buttona";
                break;
            }
            i++;
        }
        int shareLifes = dane[1] - '0';
        int yourLifes = dane[2] - '0';
        playerLifes = yourLifes;
        if(playerLifes == 0) {
            ui->hangmanGroup->setEnabled(false);
        }
        ui->shareLifesLabel->setText(QString::number(shareLifes));
        ui->yourLifesLabel->setText(QString::number(playerLifes));
        showPic();
        QByteArray wordAndRanking;
        for(i = 0; i <= dane.length(); i++) {
            wordAndRanking[i] = dane[i+1 + 2];
        }
        getWordAndRanking(wordAndRanking, started);
    }
}

void Hangman::showPic() {
    int pictureNumber = MAX_LIFES - ui->shareLifesLabel->text().toInt();
    QString filename = QString::fromStdString("./pictures/" + to_string(pictureNumber) + ".png");
    ui->picLabel->setAlignment(Qt::AlignCenter);
    QPixmap pix;

    if(pix.load(filename)) {
        pix = pix.scaled(ui->picLabel->size(), Qt::KeepAspectRatio);
        ui->picLabel->setPixmap(pix);
    }
}

void Hangman::enableButtons() {
    for(int i = 0; i < 26; i++) {
        letterButtons[i]->setEnabled(true);
    }
}

void Hangman::letterButtonHit(char c) {
    sendData(c + QString::number(QDateTime::currentMSecsSinceEpoch()) + ";");
}

void Hangman::addButonsToArray() {
    letterButtons[0] = ui->aButton;
    letterButtons[1] = ui->bButton;
    letterButtons[2] = ui->cButton;
    letterButtons[3] = ui->dButton;
    letterButtons[4] = ui->eButton;
    letterButtons[5] = ui->fButton;
    letterButtons[6] = ui->gButton;
    letterButtons[7] = ui->hButton;
    letterButtons[8] = ui->iButton;
    letterButtons[9] = ui->jButton;
    letterButtons[10] = ui->kButton;
    letterButtons[11] = ui->lButton;
    letterButtons[12] = ui->mButton;
    letterButtons[13] = ui->nButton;
    letterButtons[14] = ui->oButton;
    letterButtons[15] = ui->pButton;
    letterButtons[16] = ui->qButton;
    letterButtons[17] = ui->rButton;
    letterButtons[18] = ui->sButton;
    letterButtons[19] = ui->tButton;
    letterButtons[20] = ui->uButton;
    letterButtons[21] = ui->vButton;
    letterButtons[22] = ui->wButton;
    letterButtons[23] = ui->xButton;
    letterButtons[24] = ui->yButton;
    letterButtons[25] = ui->zButton;
}
