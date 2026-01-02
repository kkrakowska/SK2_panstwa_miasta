#pragma once
#include <QMainWindow>
#include <QTcpSocket>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QTimer>
#include "../common/protocol.hpp"
#include <QMessageBox>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
        void onConnectTimeout();
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError);

    void onCreateRoomClicked();
    void onJoinRoomClicked();
    void onStartGameClicked();
    void onLeaveRoomClicked();
    void onSubmitAnswersClicked();
    void onSubmitVotesClicked();
    void onRefreshRoomsClicked();
    void goToLobby();
    void updateFinalScoreTimer();

private:
    QTcpSocket *socket;
    QByteArray incomingBuffer;

    QStackedWidget *stackedWidget;

    QWidget *loginPage;
    QLineEdit *nickInput;
    QLineEdit *serverIpInput;
    QLineEdit *serverPortInput;
    QPushButton *connectButton;

    QWidget *lobbyPage;
    QLineEdit *roomNameInput;
    QLineEdit *roomNameInputJoin;
    QListWidget *roomList;
    QPushButton *refreshButton;
    QTextEdit *lobbyLog;

    QWidget *roomPage;
    QLabel *roomTitleLabel;
    QListWidget *playerList;
    QPushButton *startGameButton;
    QPushButton *leaveRoomButton;
    QTextEdit *gameLog;
    
    QWidget *gamePlayPage;
    QLabel *letterLabel;
    QLabel *roundLabel;
    QLabel *timeLeftLabel;
    QLineEdit *inputCountry;
    QLineEdit *inputCity;
    QLineEdit *inputAnimal;
    QLineEdit *inputPlant;
    QLineEdit *inputObject;
    QPushButton *submitButton;
    
    QWidget *verifyPage;
    QVBoxLayout *verifyLayoutContainer; 
    QPushButton *submitVotesButton;
    std::vector<std::pair<int, QCheckBox*>> voteCheckboxes; 

    QWidget *finalScorePage;
    QLabel *finalScoreLabel;
    QLabel *finalScoreTimerLabel;
    QTimer *finalScoreTimer;
    int finalScoreCountdown;

        QTimer *connectTimer;
    void setupUI();
    void processMessage(MsgHeader header, const std::vector<char>& body);
    void log(const QString &msg);
    void setupVerificationUI(const QString &data);
    void closeRoundResults();
    QWidget *roundResultsWidget;
    QLabel *roundResultsLabel;
    QTimer *roundResultsTimer;
    QWidget *roundResultsBackdrop;
};
