#include <kconfig.h>
#include <kaction.h>
#include <kedittoolbar.h>
#include <kmessagebox.h>
#include <kkeydialog.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <kglobal.h>
#include <kprinter.h>
#include <klocale.h>
#include <kmainwindow.h>
#include <kscoredialog.h>
#include <kstatusbar.h>
#include <kstdaccel.h>
#include <kstdaction.h>
#include <kstdgameaction.h>
#include <kurl.h>

#include <qcolor.h>
#include <qevent.h>
#include <qfile.h>
#include <qobject.h>
#include <qmap.h>
#include <qpoint.h>
#include <qtimer.h>
#include <qptrlist.h>
#include <qpixmap.h>
#include <qpixmapcache.h>
#include <qfileinfo.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qlayout.h>
#include <qwidget.h>

#include <stdlib.h>

#include "game.h"
#include "newgame.h"
#include "scoreboard.h"
#include "editor.h"
#include "pluginloader.h"
#include "printdialogpage.h"
#include "kolf.h"

Kolf::Kolf()
    : KMainWindow(0, "Kolf")
{
	setStandardToolBarMenuEnabled(true);

	competition = false;
	game = 0;
	editor = 0;
	spacer = 0;
	scoreboard = 0;
	isTutorial = false;

	initGUI();

	obj = new ObjectList;
	initPlugins();

	filename = QString::null;
	dummy = new QWidget(this);
	setCentralWidget(dummy);
	layout = new QGridLayout(dummy, 3, 1);

	resize(420, 480);
	applyMainWindowSettings(KGlobal::config(), "TopLevelWindow");

	closeGame();

	KConfig *config = kapp->config();
	config->setGroup("App");
	if (!config->readBoolEntry("beenRun", false))
	{
		// this isn't very kde-compliant, and now
		// that there is documentation...
		/*
		switch (KMessageBox::questionYesNo(this, i18n("Since it's your first time playing Kolf, would you like to play on the tutorial course once?")))
		{
			case KMessageBox::Yes:
				QTimer::singleShot(100, this, SLOT(tutorial()));
				break;
			case KMessageBox::No:
			default:
				break;
		}
		*/
		config->writeEntry("beenRun", true);
	}

	config->sync();
}

Kolf::~Kolf()
{
	// wipe out our objects
	obj->setAutoDelete(true);
	delete obj;
}

void Kolf::initGUI()
{
	newAction = KStdGameAction::gameNew(this, SLOT(newGame()), actionCollection());
	newAction->setText(newAction->text() + QString("..."));

	(void) KStdAction::keyBindings(this, SLOT(keyBindings()), actionCollection());
	(void) KStdAction::configureToolbars(this, SLOT(configureToolBars()), actionCollection());
	endAction = KStdGameAction::end(this, SLOT(closeGame()), actionCollection());
	printAction = KStdGameAction::print(this, SLOT(print()), actionCollection());

	(void) KStdGameAction::quit(this, SLOT(close()), actionCollection());
	saveAction = KStdAction::save(this, SLOT(save()), actionCollection(), "game_save");
	saveAction->setText(i18n("Save &Course"));
	saveAsAction = KStdAction::saveAs(this, SLOT(saveAs()), actionCollection(), "game_save_as");
	saveAsAction->setText(i18n("Save &Course As..."));

	saveGameAction = new KAction(i18n("&Save Game"), 0, this, SLOT(saveGame()), actionCollection(), "savegame");
	saveGameAsAction = new KAction(i18n("&Save Game As..."), 0, this, SLOT(saveGameAs()), actionCollection(), "savegameas");

	loadGameAction = KStdGameAction::load(this, SLOT(loadGame()), actionCollection());
	loadGameAction->setText(i18n("Load Saved..."));

	highScoreAction = KStdGameAction::highscores(this, SLOT(showHighScores()), actionCollection());

	editingAction = new KToggleAction(i18n("&Edit"), "pencil", CTRL+Key_E, this, SLOT(emptySlot()), actionCollection(), "editing");
	newHoleAction = new KAction(i18n("&New"), "filenew", CTRL+SHIFT+Key_N, this, SLOT(emptySlot()), actionCollection(), "newhole");
	clearHoleAction = new KAction(i18n("&Clear"), "locationbar_erase", CTRL+Key_Delete, this, SLOT(emptySlot()), actionCollection(), "clearhole");
	resetHoleAction = new KAction(i18n("&Reset"), CTRL+Key_R, this, SLOT(emptySlot()), actionCollection(), "resethole");
	undoShotAction = KStdAction::undo(this, SLOT(emptySlot()), actionCollection(), "undoshot");
	undoShotAction->setText(i18n("&Undo Shot"));
	//replayShotAction = new KAction(i18n("&Replay Shot"), 0, this, SLOT(emptySlot()), actionCollection(), "replay");

	holeAction = new KListAction(i18n("Switch to Hole"), 0, this, SLOT(emptySlot()), actionCollection(), "switchhole");
	nextAction = new KAction(i18n("&Next Hole"), "forward", KStdAccel::key(KStdAccel::Forward), this, SLOT(emptySlot()), actionCollection(), "nexthole");
	prevAction = new KAction(i18n("&Previous Hole"), "back", KStdAccel::key(KStdAccel::Back), this, SLOT(emptySlot()), actionCollection(), "prevhole");
	firstAction = new KAction(i18n("&First Hole"), "gohome", KStdAccel::key(KStdAccel::Home), this, SLOT(emptySlot()), actionCollection(), "firsthole");
	lastAction = new KAction(i18n("&Last Hole"), CTRL+SHIFT+Key_End, this, SLOT(emptySlot()), actionCollection(), "lasthole");
	randAction = new KAction(i18n("&Random Hole"), "goto", 0, this, SLOT(emptySlot()), actionCollection(), "randhole");

	useMouseAction = new KToggleAction(i18n("Enable &Mouse for Moving Putter"), 0, this, SLOT(emptySlot()), actionCollection(), "usemouse");
	connect(useMouseAction, SIGNAL(toggled(bool)), this, SLOT(useMouseChanged(bool)));
	KConfig *config = kapp->config();
	config->setGroup("Settings");
	useMouseAction->setChecked(config->readBoolEntry("useMouse", true));

	useAdvancedPuttingAction = new KToggleAction(i18n("Enable &Advanced Putting"), 0, this, SLOT(emptySlot()), actionCollection(), "useadvancedputting");
	connect(useAdvancedPuttingAction, SIGNAL(toggled(bool)), this, SLOT(useAdvancedPuttingChanged(bool)));
	useAdvancedPuttingAction->setChecked(config->readBoolEntry("useAdvancedPutting", false));

	showInfoAction = new KToggleAction(i18n("Show &Info"), "viewmag+", CTRL+Key_I, this, SLOT(emptySlot()), actionCollection(), "showinfo");
	connect(showInfoAction, SIGNAL(toggled(bool)), this, SLOT(showInfoChanged(bool)));
	showInfoAction->setChecked(config->readBoolEntry("showInfo", false));

	showGuideLineAction = new KToggleAction(i18n("Show Putter &Guideline"), 0, this, SLOT(emptySlot()), actionCollection(), "showguideline");
	connect(showGuideLineAction, SIGNAL(toggled(bool)), this, SLOT(showGuideLineChanged(bool)));
	showGuideLineAction->setChecked(config->readBoolEntry("showGuideLine", true));

	(void) new KToggleAction(i18n("Enable All Dialog Boxes"), 0, this, SLOT(enableAllMessages()), actionCollection(), "enableAll");

	soundAction = new KToggleAction(i18n("Play &Sounds"), 0, this, SLOT(emptySlot()), actionCollection(), "sound");
	connect(soundAction, SIGNAL(toggled(bool)), this, SLOT(soundChanged(bool)));
	soundAction->setChecked(config->readBoolEntry("sound", true));

	(void) new KAction(i18n("&Reload Plugins"), 0, this, SLOT(initPlugins()), actionCollection(), "reloadplugins");
	(void) new KAction(i18n("Show &Plugins"), 0, this, SLOT(showPlugins()), actionCollection(), "showplugins");

	aboutAction = new KAction(i18n("&About Course"), 0, this, SLOT(emptySlot()), actionCollection(), "aboutcourse");
	tutorialAction = new KAction(i18n("&Tutorial"), 0, this, SLOT(tutorial()), actionCollection(), "tutorial");

	createGUI();
}

void Kolf::closeEvent(QCloseEvent *e)
{
	if (game)
		if (game->askSave(true))
			return;
	saveMainWindowSettings(KGlobal::config(), "TopLevelWindow");
	e->accept();
}

void Kolf::startNewGame()
{
	NewGameDialog *dialog = 0;
	int firstHole = 1;

	if (loadedGame.isNull())
	{
		dialog = new NewGameDialog(filename.isNull(), dummy, "New Game Dialog");
		if (dialog->exec() != QDialog::Accepted)
			goto end;
	}

	players.clear();
	delete scoreboard;
	scoreboard = new ScoreBoard(dummy, "Score Board");
	layout->addWidget(scoreboard, 1, 0);
	scoreboard->show();

	if (loadedGame.isNull())
	{
		PlayerEditor *curEditor = 0;
		int newId = 1;
		for (curEditor = dialog->players()->first(); curEditor; curEditor = dialog->players()->next(), ++newId)
		{
			players.append(Player());
			players.last().ball()->setColor(curEditor->color());
			players.last().setName(curEditor->name());
			players.last().setId(newId);
		}

		competition = dialog->competition();
		filename = filename.isNull()? dialog->course() : filename;
	}
	else
	{
		KSimpleConfig config(loadedGame);
		config.setGroup("0 Saved Game");

		if (isTutorial)
			filename = KGlobal::dirs()->findResource("appdata", "tutorial.kolf");
		else
			filename = config.readEntry("Course", QString::null);

		if (filename.isNull())
			return;

		competition = config.readBoolEntry("Competition", false);
		firstHole = config.readNumEntry("Current Hole", 1);

		players.clear();
		KolfGame::scoresFromSaved(&config, players);
	}

	for (PlayerList::Iterator it = players.begin(); it != players.end(); ++it)
		scoreboard->newPlayer((*it).name());

	delete spacer;
	spacer = 0;
	delete game;
	game = new KolfGame(obj, &players, filename, dummy);
	game->setStrict(competition);

	connect(game, SIGNAL(newHole(int)), scoreboard, SLOT(newHole(int)));
	connect(game, SIGNAL(scoreChanged(int, int, int)), scoreboard, SLOT(setScore(int, int, int)));
	connect(game, SIGNAL(parChanged(int, int)), scoreboard, SLOT(parChanged(int, int)));
	connect(game, SIGNAL(newPlayersTurn(Player *)), this, SLOT(newPlayersTurn(Player *)));
	connect(game, SIGNAL(holesDone()), this, SLOT(gameOver()));
	connect(game, SIGNAL(checkEditing()), this, SLOT(checkEditing()));
	connect(game, SIGNAL(editingStarted()), this, SLOT(editingStarted()));
	connect(game, SIGNAL(editingEnded()), this, SLOT(editingEnded()));
	connect(game, SIGNAL(inPlayStart()), this, SLOT(inPlayStart()));
	connect(game, SIGNAL(inPlayEnd()), this, SLOT(inPlayEnd()));
	connect(game, SIGNAL(maxStrokesReached(const QString &)), this, SLOT(maxStrokesReached(const QString &)));
	connect(game, SIGNAL(largestHole(int)), this, SLOT(updateHoleMenu(int)));
	connect(game, SIGNAL(titleChanged(const QString &)), this, SLOT(titleChanged(const QString &)));
	connect(game, SIGNAL(currentHole(int)), this, SLOT(setCurrentHole(int)));
	connect(holeAction, SIGNAL(activated(const QString &)), game, SLOT(switchHole(const QString &)));
	connect(nextAction, SIGNAL(activated()), game, SLOT(nextHole()));
	connect(prevAction, SIGNAL(activated()), game, SLOT(prevHole()));
	connect(firstAction, SIGNAL(activated()), game, SLOT(firstHole()));
	connect(lastAction, SIGNAL(activated()), game, SLOT(lastHole()));
	connect(randAction, SIGNAL(activated()), game, SLOT(randHole()));
	connect(editingAction, SIGNAL(activated()), game, SLOT(toggleEditMode()));
	connect(newHoleAction, SIGNAL(activated()), game, SLOT(addNewHole()));
	connect(clearHoleAction, SIGNAL(activated()), game, SLOT(clearHole()));
	connect(resetHoleAction, SIGNAL(activated()), game, SLOT(resetHole()));
	connect(undoShotAction, SIGNAL(activated()), game, SLOT(undoShot()));
	//connect(replayShotAction, SIGNAL(activated()), game, SLOT(replay()));
	connect(aboutAction, SIGNAL(activated()), game, SLOT(showInfoDlg()));
	connect(useMouseAction, SIGNAL(toggled(bool)), game, SLOT(setUseMouse(bool)));
	connect(useAdvancedPuttingAction, SIGNAL(toggled(bool)), game, SLOT(setUseAdvancedPutting(bool)));		
	connect(soundAction, SIGNAL(toggled(bool)), game, SLOT(setSound(bool)));		
	connect(showGuideLineAction, SIGNAL(toggled(bool)), game, SLOT(setShowGuideLine(bool)));		
	connect(showInfoAction, SIGNAL(toggled(bool)), game, SLOT(setShowInfo(bool)));		

	game->setUseMouse(useMouseAction->isChecked());
	game->setUseAdvancedPutting(useAdvancedPuttingAction->isChecked());
	game->setShowInfo(showInfoAction->isChecked());
	game->setShowGuideLine(showGuideLineAction->isChecked());
	game->setSound(soundAction->isChecked());

	layout->addWidget(game, 0, 0, AlignCenter);

	game->show();
	game->setFocus();

	setEditingEnabled(true);
	endAction->setEnabled(true);
	setHoleMovementEnabled(true);
	setHoleOtherEnabled(true);
	aboutAction->setEnabled(true);
	highScoreAction->setEnabled(true);
	printAction->setEnabled(true);
	saveAction->setEnabled(true);
	saveAsAction->setEnabled(true);
	saveGameAction->setEnabled(true);
	saveGameAsAction->setEnabled(true);

	clearHoleAction->setEnabled(false);
	newHoleAction->setEnabled(false);
	newAction->setEnabled(false);
	loadGameAction->setEnabled(false);
	tutorialAction->setEnabled(false);


	// so game can do stuff that needs to be done
	// after things above are connected
	game->startFirstHole(1);

	end:
	delete dialog;
}

void Kolf::newGame()
{
	isTutorial = false;
	filename = QString::null;
	startNewGame();
}

void Kolf::tutorial()
{
	QString newfilename = KGlobal::dirs()->findResource("appdata", "tutorial.kolfgame");
	if (newfilename.isNull())
	        return;
	
	filename = QString::null;
	loadedGame = newfilename;
	isTutorial = true;

	startNewGame();

	loadedGame = QString::null;
}

void Kolf::closeGame()
{
	if (game)
	{
		if (game->askSave(true))
			return;
		game->pause();
	}

	editingEnded();
	delete game;
	game = 0;
	loadedGame = QString::null;

	editingAction->setChecked(false);
	setEditingEnabled(false);
	endAction->setEnabled(false);
	aboutAction->setEnabled(false);
	highScoreAction->setEnabled(false);
	printAction->setEnabled(false);
	saveAction->setEnabled(false);
	saveAsAction->setEnabled(false);
	saveGameAction->setEnabled(false);
	saveGameAsAction->setEnabled(false);
	setHoleMovementEnabled(false);
	setHoleOtherEnabled(false);

	clearHoleAction->setEnabled(false);
	newHoleAction->setEnabled(false);
	newAction->setEnabled(true);
	loadGameAction->setEnabled(true);
	tutorialAction->setEnabled(true);

	titleChanged("");

	QTimer::singleShot(100, this, SLOT(createSpacer()));
}

void Kolf::createSpacer()
{
	// make a player to play the spacer hole
	spacerPlayers.clear();
	spacerPlayers.append(Player());
	spacerPlayers.last().ball()->setColor(yellow);
	spacerPlayers.last().setName("player");
	spacerPlayers.last().setId(1);

	delete spacer;
	spacer = new KolfGame(obj, &spacerPlayers, KGlobal::dirs()->findResource("appdata", "intro"), dummy);
	spacer->setSound(false);
	spacer->startFirstHole(1);
	layout->addWidget(spacer, 0, 0, AlignCenter);
	spacer->hidePutter();
	spacer->ignoreEvents(true);

	spacer->show();
}

void Kolf::gameOver()
{
	if (competition)
	{
		int curPar = 0;
		int lowScore = INT_MAX; // let's hope it doesn't stay this way!
		int curScore = 1;
		QStringList names;
		HighScoreList highScores;
		int i = 1;
		HighScore topScore;

		while (curScore != 0)
		{
			QString curName;
			curScore = scoreboard->total(i, curName);

			i++;

			if (curName == i18n("Par"))
			{
				curPar = curScore;
				continue;
			}

			if (curScore == 0)
				continue;

			if (curScore < lowScore)
			{
				names.clear();
				lowScore = curScore;
				names.append(curName);
				topScore.name = curName;
				topScore.score = curScore;
			}
			else if (curScore == lowScore)
				names.append(curName);
			else
				highScores.append(HighScore(curName, curScore));
		}
		highScores.append(topScore);

		if (names.count() > 1)
		{
			QString winners = names.join(i18n(" and "));
			statusBar()->message(i18n("%1 tied").arg(winners));
		}
		else
			statusBar()->message(i18n("%1 won!").arg(names.first()));

		// deal with highscores
		// KScoreDialog makes it very easy :-))

		KScoreDialog *scoreDialog = new KScoreDialog(KScoreDialog::Name | KScoreDialog::Custom1 | KScoreDialog::Score, this);
		scoreDialog->addField(KScoreDialog::Custom1, i18n("Par"), "Par");

		CourseInfo courseInfo;
		game->courseInfo(courseInfo, game->curFilename());

		scoreDialog->setConfigGroup(courseInfo.untranslatedName + QString(" Highscores"));

		for (HighScoreList::Iterator it = highScores.begin(); it != highScores.end(); ++it)
		{
			KScoreDialog::FieldInfo info;
			info[KScoreDialog::Name] = (*it).name;
			info[KScoreDialog::Custom1] = QString::number(curPar);

			scoreDialog->addScore((*it).score, info, false, true);
		}

		scoreDialog->setComment(i18n("High Scores for %1").arg(courseInfo.name));
		scoreDialog->show();
	}

	QTimer::singleShot(700, this, SLOT(closeGame()));
}

void Kolf::showHighScores()
{
	KScoreDialog *scoreDialog = new KScoreDialog(KScoreDialog::Name | KScoreDialog::Custom1 | KScoreDialog::Score, this);
	scoreDialog->addField(KScoreDialog::Custom1, i18n("Par"), "Par");

	CourseInfo courseInfo;
	game->courseInfo(courseInfo, game->curFilename());

	scoreDialog->setConfigGroup(courseInfo.untranslatedName + QString(" Highscores"));
	scoreDialog->setComment(i18n("High Scores for %1").arg(courseInfo.name));
	scoreDialog->show();
}

void Kolf::save()
{
	if (filename.isNull())
	{
		saveAs();
		return;
	}

	if (game)
		game->save();

	game->setFocus();
}

void Kolf::saveAs()
{
	QString newfilename = KFileDialog::getSaveFileName(QString::null, "application/x-kourse", this, i18n("Pick Kolf Course to Save To"));
	if (!newfilename.isNull())
	{
		filename = newfilename;
		game->setFilename(filename);
		game->save();
		game->setFocus();
	}
}

void Kolf::saveGameAs()
{
	QString newfilename = KFileDialog::getSaveFileName(QString::null, "application/x-kolf", this, i18n("Pick Saved Game to Save To"));
	if (newfilename.isNull())
		return;
	
	loadedGame = newfilename;
	
	saveGame();
}

void Kolf::saveGame()
{
	if (loadedGame.isNull())
	{
		saveGameAs();
		return;
	}
	
	KSimpleConfig config(loadedGame);
	config.setGroup("0 Saved Game");

	config.writeEntry("Competition", competition);
	config.writeEntry("Course", filename);

	game->saveScores(&config);

	config.sync();
}

void Kolf::loadGame()
{
	loadedGame = KFileDialog::getOpenFileName(QString::null, QString::fromLatin1("application/x-kolf"), this, i18n("Pick Kolf Saved Game"));

	if (loadedGame.isNull())
		return;

	isTutorial = false;
	startNewGame();
}

void Kolf::newPlayersTurn(Player *player)
{
	statusBar()->message(i18n("%1's turn").arg(player->name()));
	scoreboard->setCurrentCell(player->id() - 1, game->currentHole() - 1);
}

void Kolf::editingStarted()
{
	delete editor;
	editor = new Editor(obj, dummy, "Editor");
	connect(editor, SIGNAL(addNewItem(Object *)), game, SLOT(addNewObject(Object *)));
	connect(editor, SIGNAL(changed()), game, SLOT(setModified()));
	connect(editor, SIGNAL(addNewItem(Object *)), this, SLOT(setHoleFocus()));
	connect(game, SIGNAL(newSelectedItem(CanvasItem *)), editor, SLOT(setItem(CanvasItem *)));

	scoreboard->hide();

	layout->addWidget(editor, 1, 0);
	editor->show();

	clearHoleAction->setEnabled(true);
	newHoleAction->setEnabled(true);
	setHoleOtherEnabled(false);

	game->setFocus();
}

void Kolf::editingEnded()
{
	delete editor;
	editor = 0;

	if (scoreboard)
		scoreboard->show();

	clearHoleAction->setEnabled(false);
	newHoleAction->setEnabled(false);
	setHoleOtherEnabled(true);

	if (game)
		game->setFocus();
}

void Kolf::inPlayStart()
{
	setEditingEnabled(false);
	setHoleOtherEnabled(false);
	setHoleMovementEnabled(false);
}

void Kolf::inPlayEnd()
{
	setEditingEnabled(true);
	setHoleOtherEnabled(true);
	setHoleMovementEnabled(true);
}

void Kolf::maxStrokesReached(const QString &name)
{
	KMessageBox::sorry(this, i18n("%1's score has reached the maximum for this hole.").arg(name));
}

void Kolf::updateHoleMenu(int largest)
{
	QStringList items;
	for (int i = 1; i <= largest; ++i)
		items.append(QString::number(i));

	// setItems for some reason enables the action
	bool shouldbe = holeAction->isEnabled();
	holeAction->setItems(items);
	holeAction->setEnabled(shouldbe);
}

void Kolf::setHoleMovementEnabled(bool yes)
{
	if (competition)
		yes = false;

	holeAction->setEnabled(yes);

	nextAction->setEnabled(yes);
	prevAction->setEnabled(yes);
	firstAction->setEnabled(yes);
	lastAction->setEnabled(yes);
	randAction->setEnabled(yes);
}

void Kolf::setHoleOtherEnabled(bool yes)
{
	if (competition)
		yes = false;

	resetHoleAction->setEnabled(yes);
	undoShotAction->setEnabled(yes);
	//replayShotAction->setEnabled(yes);
}

void Kolf::setEditingEnabled(bool yes)
{
	editingAction->setEnabled(competition? false : yes);
}

void Kolf::checkEditing()
{
	editingAction->setChecked(true);
}

void Kolf::print()
{
	KPrinter pr;
	pr.addDialogPage(new PrintDialogPage());

	if (pr.setup())
	{
		pr.newPage();
		if (game)
			game->print(pr);
	}
}

void Kolf::titleChanged(const QString &newTitle)
{
	setCaption(newTitle);
}

void Kolf::useMouseChanged(bool yes)
{
	KConfig *config = kapp->config(); config->setGroup("Settings"); config->writeEntry("useMouse", yes); config->sync();
}

void Kolf::useAdvancedPuttingChanged(bool yes)
{
	KConfig *config = kapp->config(); config->setGroup("Settings"); config->writeEntry("useAdvancedPutting", yes); config->sync();
}

void Kolf::showInfoChanged(bool yes)
{
	KConfig *config = kapp->config(); config->setGroup("Settings"); config->writeEntry("showInfo", yes); config->sync();
}

void Kolf::showGuideLineChanged(bool yes)
{
	KConfig *config = kapp->config(); config->setGroup("Settings"); config->writeEntry("showGuideLine", yes); config->sync();
}

void Kolf::soundChanged(bool yes)
{
	KConfig *config = kapp->config(); config->setGroup("Settings"); config->writeEntry("sound", yes); config->sync();
}

void Kolf::initPlugins()
{
	//kdDebug() << "initPlugins" << endl;
	if (game)
		game->pause();

	obj->setAutoDelete(true);
	obj->clear();
	plugins.setAutoDelete(false);
	plugins.clear();

	// add prefab objects
	obj->append(new SlopeObj());
	obj->append(new PuddleObj());
	obj->append(new WallObj());
	obj->append(new CupObj());
	obj->append(new SandObj());
	obj->append(new WindmillObj());
	obj->append(new BlackHoleObj());
	obj->append(new FloaterObj());
	obj->append(new BridgeObj());
	obj->append(new SignObj());
	obj->append(new BumperObj());

	ObjectList *other = PluginLoader::loadAll();
	Object *object = 0;
	for (object = other->first(); object; object = other->next())
	{
		obj->append(object);
		plugins.append(object);
	}

	if (game)
	{
		game->setObjects(obj);
		game->unPause();
	}

	//kdDebug() << "end of initPlugins" << endl;
}

void Kolf::showPlugins()
{
	QString text = QString("<h2>%1</h2><ol>").arg(i18n("Currently Loaded Plugins"));
	Object *object = 0;
	for (object = plugins.first(); object; object = plugins.next())
	{
		text.append("<li>");
		text.append(object->name());
		text.append(" - ");
		text.append(i18n("by %1").arg(object->author()));
		text.append("</li>");
	}
	text.append("</ol>");
	KMessageBox::information(this, text, i18n("Plugins"));
}

void Kolf::keyBindings()
{
	KKeyDialog::configure(actionCollection());
}

void Kolf::configureToolBars()
{
	saveMainWindowSettings(KGlobal::config(), "TopLevelWindow");

	KEditToolbar dlg(actionCollection());
	connect(&dlg, SIGNAL(newToolbarConfig()), SLOT(newToolBarConfig()));

	if (dlg.exec())
		createGUI();
}

void Kolf::newToolBarConfig()
{
	applyMainWindowSettings(KGlobal::config(), "TopLevelWindow");
}

void Kolf::enableAllMessages()
{
	KMessageBox::enableAllMessages();
}

void Kolf::setCurrentHole(int hole)
{
	if(!holeAction) return;
	// Golf is 1-based, KListAction is 0-based
	holeAction->setCurrentItem(hole - 1);
}

#include "kolf.moc"
