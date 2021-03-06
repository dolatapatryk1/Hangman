#ifndef HANGMAN_H
#define HANGMAN_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTimer>
#include <QtGui>
#include<QtWidgets>
#include<QtNetwork>
#include <array>
using namespace std;

namespace Ui {
class Hangman;
}

class Hangman : public QMainWindow
{
    Q_OBJECT

public:
    explicit Hangman(QWidget *parent = nullptr);
    ~Hangman();

protected:
    QTcpSocket * sock;
    QTimer * connTimeoutTimer;
    QPushButton* letterButtons[26];
    bool started = false;
    char GAME_STARTED = '1';
    char READY = '1';
    char GET_FD = '2';
    char GET_RANKING = '3';
    char GAME_ENDED = '0';
    char GAME_ALREADY_STARTED = '4';
    const int MAX_LIFES = 9;
    int playerLifes;
    void connectButtonHit();
    void socketConnected();
    void readData();
    void readyButtonHit();
    void sendData(char c);
    void sendData(QString s);
    void getWord(QByteArray dane);
    void getFd(QByteArray dane);
    void getRanking(QByteArray dane);
    void getFdAndRanking(QByteArray dane);
    void getWordAndRanking(QByteArray dane, bool isStarted);
    void gameEnded();
    void gameEndedAndRanking(QByteArray dane);
    void gameAlreadyStarted(QByteArray dane);
    void disableButton(QByteArray dane);
    void enableButtons();
    void addButonsToArray();
    void showPic();
    void letterButtonHit(char c);
    void aButtonHit();
    void bButtonHit();
    void cButtonHit();
    void dButtonHit();
    void eButtonHit();
    void fButtonHit();
    void gButtonHit();
    void hButtonHit();
    void iButtonHit();
    void jButtonHit();
    void kButtonHit();
    void lButtonHit();
    void mButtonHit();
    void nButtonHit();
    void oButtonHit();
    void pButtonHit();
    void qButtonHit();
    void rButtonHit();
    void sButtonHit();
    void tButtonHit();
    void uButtonHit();
    void vButtonHit();
    void wButtonHit();
    void xButtonHit();
    void yButtonHit();
    void zButtonHit();
    array<int, 2> getMessageLengthAndShift(QByteArray dane);
private:
    Ui::Hangman *ui;
    std::string picspath = "./pics/";
};

#endif // HANGMAN_H
