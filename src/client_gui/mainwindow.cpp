#include "mainwindow.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollArea>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();

    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onError(QAbstractSocket::SocketError)));
    roundResultsBackdrop = nullptr;
    roundResultsWidget = nullptr;
    roundResultsLabel = nullptr;
    roundResultsTimer = new QTimer(this);
    roundResultsTimer->setSingleShot(true);
    connect(roundResultsTimer, &QTimer::timeout, this, &MainWindow::closeRoundResults);

    connectTimer = new QTimer(this);
    connectTimer->setSingleShot(true);
    connect(connectTimer, &QTimer::timeout, this, &MainWindow::onConnectTimeout);
}

void MainWindow::onError(QAbstractSocket::SocketError) {
    if (stackedWidget->currentIndex() == 0) {
         QMessageBox::critical(this, "Błąd", socket->errorString());
         connectButton->setEnabled(true);
         connectButton->setText("Graj!");
         connectTimer->stop();
    }
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);
    resize(500, 700);

    loginPage = new QWidget();
    QVBoxLayout *loginLayout = new QVBoxLayout(loginPage);
    
    QLabel *title = new QLabel("PANSTWA - MIASTA", loginPage);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 24px; font-weight: bold; margin-bottom: 20px;");
    
    nickInput = new QLineEdit(loginPage);
    nickInput->setPlaceholderText("Wpisz swoj nick...");
    
    serverIpInput = new QLineEdit(loginPage);
    serverIpInput->setPlaceholderText("Server IP (default 127.0.0.1)");
    serverIpInput->setText("127.0.0.1");

    serverPortInput = new QLineEdit(loginPage);
    serverPortInput->setPlaceholderText("Port (default 12345)");
    serverPortInput->setText("12345");
    serverPortInput->setMaximumWidth(120);
    
    connectButton = new QPushButton("Graj!", loginPage);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);

    loginLayout->addStretch();
    loginLayout->addWidget(title);
    loginLayout->addWidget(nickInput);
    QHBoxLayout *addrLayout = new QHBoxLayout();
    addrLayout->addWidget(serverIpInput);
    addrLayout->addWidget(serverPortInput);
    loginLayout->addLayout(addrLayout);
    loginLayout->addWidget(connectButton);
    loginLayout->addStretch();

    lobbyPage = new QWidget();
    QVBoxLayout *lobbyLayout = new QVBoxLayout(lobbyPage);

    QGroupBox *createGroup = new QGroupBox("Stwórz Pokój", lobbyPage);
    QVBoxLayout *createLayout = new QVBoxLayout(createGroup);
    roomNameInput = new QLineEdit();
    roomNameInput->setPlaceholderText("Nazwa pokoju");
    QPushButton *createBtn = new QPushButton("Stwórz");
    connect(createBtn, &QPushButton::clicked, this, &MainWindow::onCreateRoomClicked);
    createLayout->addWidget(roomNameInput);
    createLayout->addWidget(createBtn);

    QGroupBox *joinGroup = new QGroupBox("Dołącz do Pokoju", lobbyPage);
    QVBoxLayout *joinLayout = new QVBoxLayout(joinGroup);
    roomNameInputJoin = new QLineEdit();
    roomNameInputJoin->setPlaceholderText("Nazwa Pokoju");
    QPushButton *joinBtn = new QPushButton("Dołącz");
    connect(joinBtn, &QPushButton::clicked, this, &MainWindow::onJoinRoomClicked);
    joinLayout->addWidget(roomNameInputJoin);
    joinLayout->addWidget(joinBtn);

    roomList = new QListWidget();
    refreshButton = new QPushButton("Odśwież listę pokoi");
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshRoomsClicked);

    lobbyLog = new QTextEdit();
    lobbyLog->setReadOnly(true);

    lobbyLayout->addWidget(createGroup);
    lobbyLayout->addWidget(joinGroup);
    lobbyLayout->addWidget(new QLabel("Dostępne pokoje:"));
    lobbyLayout->addWidget(roomList);
    lobbyLayout->addWidget(refreshButton);
    lobbyLayout->addWidget(new QLabel("Logi:"));
    lobbyLayout->addWidget(lobbyLog);

    roomPage = new QWidget();
    QVBoxLayout *roomLayout = new QVBoxLayout(roomPage);

    roomTitleLabel = new QLabel("Pokój: ???");
    roomTitleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    roomTitleLabel->setAlignment(Qt::AlignCenter);

    playerList = new QListWidget();
    
    startGameButton = new QPushButton("START GRY (Tylko Host)");
    startGameButton->setEnabled(false);
    connect(startGameButton, &QPushButton::clicked, this, &MainWindow::onStartGameClicked);

    leaveRoomButton = new QPushButton("Opuść pokój");
    connect(leaveRoomButton, &QPushButton::clicked, this, &MainWindow::onLeaveRoomClicked);

    gameLog = new QTextEdit();
    gameLog->setReadOnly(true);

    roomLayout->addWidget(roomTitleLabel);
    roomLayout->addWidget(new QLabel("Gracze:"));
    roomLayout->addWidget(playerList);
    roomLayout->addWidget(startGameButton);
    roomLayout->addWidget(leaveRoomButton);
    roomLayout->addWidget(new QLabel("Przebieg gry:"));
    roomLayout->addWidget(gameLog);
    
    gamePlayPage = new QWidget();
    QVBoxLayout *gameLayout = new QVBoxLayout(gamePlayPage);
    
    letterLabel = new QLabel("Litera: ?");
    letterLabel->setStyleSheet("font-size: 40px; font-weight: bold; color: blue;");
    letterLabel->setAlignment(Qt::AlignCenter);
    
    roundLabel = new QLabel("Runda: ?/?");
    roundLabel->setStyleSheet("font-size: 20px; font-weight: bold;");
    roundLabel->setAlignment(Qt::AlignCenter);
    
    timeLeftLabel = new QLabel("Czas: 30s");
    timeLeftLabel->setStyleSheet("font-size: 20px; color: red;");
    timeLeftLabel->setAlignment(Qt::AlignCenter);
    
    QFormLayout *form = new QFormLayout();
    inputCountry = new QLineEdit();
    inputCity = new QLineEdit();
    inputAnimal = new QLineEdit();
    inputPlant = new QLineEdit();
    inputObject = new QLineEdit();
    
    form->addRow("Państwo:", inputCountry);
    form->addRow("Miasto:", inputCity);
    form->addRow("Zwierzę:", inputAnimal);
    form->addRow("Roślina:", inputPlant);
    form->addRow("Rzecz:", inputObject);
    
    submitButton = new QPushButton("Wyślij Odpowiedzi!");
    submitButton->setStyleSheet("background-color: green; color: white; font-weight: bold; padding: 10px;");
    connect(submitButton, &QPushButton::clicked, this, &MainWindow::onSubmitAnswersClicked);
    
    gameLayout->addWidget(letterLabel);
    gameLayout->addWidget(roundLabel);
    gameLayout->addWidget(timeLeftLabel);
    gameLayout->addLayout(form);
    gameLayout->addWidget(submitButton);
    gameLayout->addStretch();
    
    verifyPage = new QWidget();
    QVBoxLayout *mainVerifyLayout = new QVBoxLayout(verifyPage);
    
    QLabel *verLabel = new QLabel("Zweryfikuj odpowiedzi innych graczy\nZaznacz BŁĘDNE odpowiedzi");
    verLabel->setAlignment(Qt::AlignCenter);
    verLabel->setStyleSheet("font-weight: bold; color: red;");
    
    QScrollArea *scroll = new QScrollArea();
    QWidget *scrollContent = new QWidget();
    verifyLayoutContainer = new QVBoxLayout(scrollContent);
    scroll->setWidget(scrollContent);
    scroll->setWidgetResizable(true);
    
    submitVotesButton = new QPushButton("Zatwierdź głosy");
    submitVotesButton->setStyleSheet("background-color: blue; color: white; padding: 10px;");
    connect(submitVotesButton, &QPushButton::clicked, this, &MainWindow::onSubmitVotesClicked);
    
    mainVerifyLayout->addWidget(verLabel);
    mainVerifyLayout->addWidget(scroll);
    mainVerifyLayout->addWidget(submitVotesButton);

    finalScorePage = new QWidget();
    QVBoxLayout *finalLayout = new QVBoxLayout(finalScorePage);
    finalScoreLabel = new QLabel();
    finalScoreLabel->setAlignment(Qt::AlignCenter);
    finalScoreLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    finalScoreTimerLabel = new QLabel("Powrót do lobby za: 15s");
    finalScoreTimerLabel->setAlignment(Qt::AlignCenter);
    finalLayout->addWidget(finalScoreLabel);
    finalLayout->addWidget(finalScoreTimerLabel);
    finalLayout->addStretch();
    
    finalScoreTimer = new QTimer(this);
    connect(finalScoreTimer, &QTimer::timeout, this, &MainWindow::updateFinalScoreTimer);

    stackedWidget->addWidget(loginPage);
    stackedWidget->addWidget(lobbyPage);
    stackedWidget->addWidget(roomPage);
    stackedWidget->addWidget(gamePlayPage);
    stackedWidget->addWidget(verifyPage);
    stackedWidget->addWidget(finalScorePage);

    stackedWidget->setCurrentIndex(0);
}

void MainWindow::log(const QString &msg) {
    if (stackedWidget->currentIndex() == 1) lobbyLog->append(msg);
    else if (stackedWidget->currentIndex() == 2) gameLog->append(msg);
}

void MainWindow::onConnectClicked() {
    QString nick = nickInput->text();
    if (nick.isEmpty()) return;
    
    connectButton->setEnabled(false);
    QString host = serverIpInput->text().trimmed();
    if (host.isEmpty()) host = "127.0.0.1";
    int port = serverPortInput->text().toInt();
    if (port <= 0) port = 12345;
    socket->connectToHost(host, static_cast<quint16>(port));
    connectButton->setText("Łączenie...");
    connectTimer->start(8000);
}

void MainWindow::onConnectTimeout() {
    socket->abort();
    connectTimer->stop();
    connectButton->setEnabled(true);
    connectButton->setText("Graj!");
    if (stackedWidget && stackedWidget->currentIndex() == 0) {
        QMessageBox::warning(this, "Błąd", "Przekroczono czas łączenia z serwerem.");
    }
}


void MainWindow::onCreateRoomClicked() {
    QString name = roomNameInput->text();
    if (name.isEmpty()) return;
    
    std::string data = name.toStdString();
    auto msg = createMessage(MsgType::CREATE_ROOM, data);
    socket->write(msg.data(), msg.size());
}

void MainWindow::onJoinRoomClicked() {
    QString name = roomNameInputJoin->text();
    if (name.isEmpty()) return;
    
    std::string data = name.toStdString();
    auto msg = createMessage(MsgType::JOIN_ROOM, data);
    socket->write(msg.data(), msg.size());
}

void MainWindow::onStartGameClicked() {
    auto msg = createMessage(MsgType::START_GAME, "");
    socket->write(msg.data(), msg.size());
}

void MainWindow::onLeaveRoomClicked() {
    auto msg = createMessage(MsgType::LEAVE_ROOM, "");
    socket->write(msg.data(), msg.size());
    goToLobby();
}

void MainWindow::onSubmitAnswersClicked() {
    QString answers = inputCountry->text() + ";" + 
                      inputCity->text() + ";" + 
                      inputAnimal->text() + ";" + 
                      inputPlant->text() + ";" + 
                      inputObject->text();
                      
    std::string data = answers.toStdString();
    auto msg = createMessage(MsgType::SUBMIT_ANSWERS, data);
    socket->write(msg.data(), msg.size());
    
    submitButton->setEnabled(false);
    submitButton->setText("Wysłano! Czekaj na innych...");
}

void MainWindow::setupVerificationUI(const QString &data) {
    QLayoutItem *item;
    while ((item = verifyLayoutContainer->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    voteCheckboxes.clear();
    
    QStringList categories = data.split(";");
    int catIdx = 0;
    
    for (const QString &catStr : categories) {
        if (catStr.trimmed().isEmpty()) continue;
        
        QStringList parts = catStr.split(":");
        if (parts.size() < 2) continue;
        
        QString catName = parts[0];
        QStringList answers = parts[1].split(",");
        
        QLabel *catLabel = new QLabel(catName);
        catLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
        verifyLayoutContainer->addWidget(catLabel);
        
        for (const QString &ans : answers) {
            if (ans.trimmed().isEmpty()) continue;
            QCheckBox *cb = new QCheckBox(ans);
            verifyLayoutContainer->addWidget(cb);
            voteCheckboxes.push_back({catIdx, cb});
        }
        catIdx++;
    }
    
    verifyLayoutContainer->addStretch();
}

void MainWindow::onSubmitVotesClicked() {
    QString voteData = "";
    
    for (auto &pair : voteCheckboxes) {
        if (pair.second->isChecked()) {
            voteData += QString::number(pair.first) + ":" + pair.second->text() + ";";
        }
    }
    
    std::string data = voteData.toStdString();
    auto msg = createMessage(MsgType::SEND_VOTE, data);
    socket->write(msg.data(), msg.size());
    
    submitVotesButton->setEnabled(false);
    submitVotesButton->setText("Głosy wysłane. Czekaj na wyniki...");
}

void MainWindow::onRefreshRoomsClicked() {
    auto msg = createMessage(MsgType::GET_ROOM_LIST, "");
    socket->write(msg.data(), msg.size());
}

void MainWindow::goToLobby() {
    finalScoreTimer->stop();
    stackedWidget->setCurrentIndex(1);
    playerList->clear();
    lobbyLog->clear();
    gameLog->clear();
    submitButton->setEnabled(true);
    submitButton->setText("Wyślij Odpowiedzi!");
    inputCountry->clear();
    inputCity->clear();
    inputAnimal->clear();
    inputPlant->clear();
    if (roundResultsWidget) {
        roundResultsWidget->close();
        roundResultsWidget = nullptr;
        roundResultsLabel = nullptr;
    }
    if (roundResultsBackdrop) {
        roundResultsBackdrop->close();
        roundResultsBackdrop = nullptr;
    }
}
void MainWindow::onConnected() {
    if (connectTimer) connectTimer->stop();

    std::string nick = nickInput->text().toStdString();
    auto msg = createMessage(MsgType::LOGIN, nick);
    socket->write(msg.data(), msg.size());
}

void MainWindow::onDisconnected() {
    if (stackedWidget->currentIndex() != 0) {
        QMessageBox::critical(this, "Rozłączono", "Połączenie z serwerem przerwane.");
    }
    stackedWidget->setCurrentIndex(0);
    connectButton->setEnabled(true);
    playerList->clear();
    lobbyLog->clear();
    gameLog->clear();
    
    submitButton->setEnabled(true);
    submitButton->setText("Wyślij Odpowiedzi!");
    inputCountry->clear();
    if (roundResultsWidget) {
        roundResultsWidget->close();
        roundResultsWidget = nullptr;
        roundResultsLabel = nullptr;
    }
    if (roundResultsBackdrop) {
        roundResultsBackdrop->close();
        roundResultsBackdrop = nullptr;
    }
}

void MainWindow::onReadyRead() {
    QByteArray data = socket->readAll();
    incomingBuffer.append(data);

    while (true) {
        if (incomingBuffer.size() < (int)sizeof(MsgHeader)) break;

        MsgHeader *header = reinterpret_cast<MsgHeader*>(incomingBuffer.data());
        uint32_t len = ntohl(header->len);

        if (incomingBuffer.size() < (int)(sizeof(MsgHeader) + len)) break;

        std::vector<char> body;
        if (len > 0) {
            body.resize(len);
            std::memcpy(body.data(), incomingBuffer.data() + sizeof(MsgHeader), len);
        }

        MsgHeader currentHeader = *header;
        incomingBuffer.remove(0, sizeof(MsgHeader) + len);
        processMessage(currentHeader, body);
    }
}

void MainWindow::processMessage(MsgHeader header, const std::vector<char>& body) {
    std::string msgData(body.begin(), body.end());
    QString text = QString::fromStdString(msgData);

    switch (header.type) {
        case MsgType::LOGIN_OK:
            stackedWidget->setCurrentIndex(1);
            log("Witaj w lobby: " + nickInput->text());
            onRefreshRoomsClicked();
            break;
            
        case MsgType::LOGIN_FAIL:
            QMessageBox::warning(this, "Błąd logowania", text);
            socket->disconnectFromHost();
            break;



        case MsgType::CREATE_ROOM_OK:
            stackedWidget->setCurrentIndex(2);
            roomTitleLabel->setText("Pokój: " + text);
            startGameButton->setEnabled(false);
            playerList->clear();
            playerList->addItem(nickInput->text() + " (Ty)");
            log("Utworzono pokój.");
            break;

        case MsgType::CREATE_ROOM_FAIL:
            QMessageBox::warning(this, "Błąd tworzenia pokoju", text);
            break;

        case MsgType::JOIN_ROOM_OK: {
            stackedWidget->setCurrentIndex(2);
            QStringList parts = text.split(";");
            if (parts.size() >= 2) {
                roomTitleLabel->setText("Pokój: " + parts[0]);
                QStringList nicks = parts[1].split(",");
                for (const QString &n : nicks) {
                    if (n == nickInput->text()) {
                        playerList->addItem(n + " (Ty)");
                    } else {
                        playerList->addItem(n);
                    }
                }
            }
            startGameButton->setEnabled(false);
            log("Dołączono do pokoju.");
            break;
        }
            
        case MsgType::JOIN_ROOM_FAIL:
            QMessageBox::warning(this, "Błąd", "Nie udało się dołączyć: " + text);
            break;

        case MsgType::NEW_PLAYER_JOINED:
            playerList->addItem(text);
            startGameButton->setEnabled(true);
            log("Gracz dołączył: " + text);
            break;

        case MsgType::PLAYER_LEFT:
            for (int i = 0; i < playerList->count(); ++i) {
                if (playerList->item(i)->text().startsWith(text)) {
                    delete playerList->takeItem(i);
                    break;
                }
            }
            log("Gracz opuścił: " + text);
            break;

        case MsgType::ROOM_LIST: {
            roomList->clear();
            QStringList rooms = text.split(";");
            for (const QString &room : rooms) {
                if (room.trimmed().isEmpty()) continue;

                QStringList parts = room.split(":");
                QString name;
                QString state;
                if (parts.size() >= 4) {
                    name = parts[1];
                    state = parts[3];
                } else if (parts.size() >= 2) {
                    name = parts[0];
                    state = parts[1];
                } else {
                    name = room;
                    state = "waiting";
                }

                QString stateText = (state == "inprogress" || state.toLower() == "inprogress") ? "In progress" : "Waiting";
                QString display = name + " | " + stateText;
                roomList->addItem(display);
            }
            break;
        }
        
        case MsgType::VERIFICATION_START:
            setupVerificationUI(text);
            stackedWidget->setCurrentIndex(4);
            submitVotesButton->setEnabled(true);
            submitVotesButton->setText("Zatwierdź głosy");
            break;

        case MsgType::GAME_STARTED: {
            QStringList parts = text.split(";");
            if (parts.size() >= 3) {
                QString letter = parts[0];
                int current = parts[1].toInt();
                int maxR = parts[2].toInt();
                letterLabel->setText("Litera: " + letter);
                roundLabel->setText("Runda: " + QString::number(current) + "/" + QString::number(maxR));
                timeLeftLabel->setText("Czas: 30s");
            }

            if (roundResultsWidget) {
                roundResultsWidget->close();
                roundResultsWidget = nullptr;
                roundResultsLabel = nullptr;
            }
            if (roundResultsBackdrop) {
                roundResultsBackdrop->close();
                roundResultsBackdrop = nullptr;
            }
            roundResultsTimer->stop();

            inputCountry->clear();
            inputCity->clear();
            inputAnimal->clear();
            inputPlant->clear();
            inputObject->clear();
            submitButton->setEnabled(true);
            submitButton->setText("Wyślij Odpowiedzi!");

            stackedWidget->setCurrentIndex(3);
            break;
        }
            
        case MsgType::TIME_UP:
            onSubmitAnswersClicked();
            break;
            
        case MsgType::TIME_LEFT:
            timeLeftLabel->setText("Czas: " + QString::number(std::stoi(text.toStdString())) + "s");
            break;
            
        case MsgType::ROUND_END: {
            QStringList results = text.split(";");
            QString message = "WYNIKI RUNDY:\n";
            QString display = "";
            for (const QString &r : results) {
                if (r.trimmed().isEmpty()) continue;
                QStringList kv = r.split(":");
                if (kv.size() >= 2) {
                    QString nick = kv[0];
                    QString pts = kv[1];
                    display += nick + ": " + pts + " pkt\n";
                } else {
                    display += r + "\n";
                }
            }

            if (roundResultsWidget) {
                roundResultsWidget->close();
                roundResultsWidget = nullptr;
                roundResultsLabel = nullptr;
            }
            if (roundResultsBackdrop) {
                roundResultsBackdrop->close();
                roundResultsBackdrop = nullptr;
            }

            roundResultsBackdrop = new QWidget(this);
            roundResultsBackdrop->setAttribute(Qt::WA_DeleteOnClose);
            roundResultsBackdrop->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
            roundResultsBackdrop->setStyleSheet("background-color: rgba(0,0,0,160);");
            roundResultsBackdrop->resize(this->size());
            roundResultsBackdrop->move(0, 0);
            roundResultsBackdrop->show();

            roundResultsWidget = new QWidget(roundResultsBackdrop);
            roundResultsWidget->setAttribute(Qt::WA_DeleteOnClose);
            roundResultsWidget->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
            roundResultsWidget->setStyleSheet("background-color: rgba(40,40,40,240); border-radius: 10px;");
            QVBoxLayout *v = new QVBoxLayout(roundResultsWidget);
            roundResultsLabel = new QLabel(display, roundResultsWidget);
            roundResultsLabel->setWordWrap(true);
            roundResultsLabel->setAlignment(Qt::AlignCenter);
            QFont f = roundResultsLabel->font();
            f.setPointSize(qMax(14, f.pointSize() + 4));
            f.setBold(true);
            roundResultsLabel->setFont(f);
            roundResultsLabel->setStyleSheet("color: white; padding: 16px;");
            v->addWidget(roundResultsLabel);

            QSize sz = roundResultsWidget->sizeHint();
            int w = qMax(360, sz.width());
            int h = qMax(120, sz.height());
            roundResultsWidget->resize(w, h);
            QPoint center = this->rect().center();
            roundResultsWidget->move(center.x() - w/2, center.y() - h/2 - 30);
            roundResultsWidget->show();

            roundResultsTimer->start(5000);

            stackedWidget->setCurrentIndex(2);
            if (startGameButton->isEnabled() == false && roomTitleLabel->text().contains("ID:")) {
                startGameButton->setEnabled(true);
            }
            break;
        }
            
        case MsgType::GAME_END: {
            QStringList results = text.split(";");
            QString message = "KONIEC GRY - WYNIKI KOŃCOWE:\n";
            for (const QString &r : results) {
                if (!r.isEmpty()) message += r + " pkt\n";
            }
            if (roundResultsWidget) {
                roundResultsWidget->close();
                roundResultsWidget = nullptr;
                roundResultsLabel = nullptr;
            }
            if (roundResultsBackdrop) {
                roundResultsBackdrop->close();
                roundResultsBackdrop = nullptr;
            }
            roundResultsTimer->stop();

            finalScoreLabel->setText(message);
            stackedWidget->setCurrentIndex(5);
            finalScoreCountdown = 15;
            finalScoreTimer->start(1000);
            updateFinalScoreTimer();
            break;
        }
            
        case MsgType::GAME_START_FAIL:
            QMessageBox::warning(this, "Błąd startu", text);
            break;

        default:
            break;
    }
}

void MainWindow::closeRoundResults() {
    if (roundResultsWidget) {
        roundResultsWidget->close();
        roundResultsWidget = nullptr;
        roundResultsLabel = nullptr;
    }
    if (roundResultsBackdrop) {
        roundResultsBackdrop->close();
        roundResultsBackdrop = nullptr;
    }
}

void MainWindow::updateFinalScoreTimer() {
    finalScoreCountdown--;
    if (finalScoreCountdown <= 0) {
        goToLobby();
    } else {
        finalScoreTimerLabel->setText("Powrót do lobby za: " + QString::number(finalScoreCountdown) + "s");
    }
}
