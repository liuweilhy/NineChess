<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de_DE">
<context>
    <name>GameController</name>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1170"/>
        <source>当前正在浏览历史局面。</source>
        <translation>Sie betrachten gerade eine frühere Stellung.</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1171"/>
        <source>是否在此局面下重新开始？悔棋者将承担时间损失！</source>
        <translation>Von dieser Stellung aus neu beginnen? Wer einen Zug zurücknimmt, verliert Zeit!</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1174"/>
        <source>确定</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1175"/>
        <source>取消</source>
        <translation>Abbrechen</translation>
    </message>
</context>
<context>
    <name>NineChess</name>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="178"/>
        <source>成三棋</source>
        <translation>Cheng San Qi</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="179"/>
        <source>打三棋(12连棋)</source>
        <translation>Da San Qi (12 Steine)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="180"/>
        <source>九连棋</source>
        <translation>NineChess</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="181"/>
        <source>莫里斯九子棋</source>
        <translation>Mühle</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="184"/>
        <source>1. 双方各9颗子，开局依次摆子；
2. 凡出现三子相连，就提掉对手一子；
3. 不能提对手的“三连”子，除非无子可提；
4. 同时出现两个“三连”只能提一子；
5. 摆完后依次走子，每次只能往相邻位置走一步；
6. 把对手棋子提到少于3颗时胜利；
7. 走棋阶段不能行动（被“闷”）算负。</source>
        <translation>1. Jede Seite hat 9 Steine, die zu Beginn abwechselnd gesetzt werden;
2. Drei Steine in einer Reihe entfernen einen gegnerischen Stein;
3. Ein Stein einer gegnerischen Dreierreihe darf nur entfernt werden, wenn kein anderer Stein verfügbar ist;
4. Entstehen gleichzeitig zwei Dreierreihen, darf nur ein Stein entfernt werden;
5. Nach dem Setzen wird abwechselnd gezogen, jeweils einen Schritt auf einen benachbarten Punkt;
6. Wer den Gegner auf weniger als 3 Steine reduziert, gewinnt;
7. Wer in der Zugphase nicht ziehen kann (blockiert ist), verliert.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="192"/>
        <source>1. 双方各12颗子，棋盘有斜线；
2. 摆棋阶段被提子的位置不能再摆子，直到走棋阶段；
3. 摆棋阶段，摆满棋盘算先手负；
4. 走棋阶段，后摆棋的一方先走；
5. 一步出现几个“三连”就可以提几个子；
6. 其它规则与成三棋基本相同。</source>
        <translation>1. Jede Seite hat 12 Steine, und das Brett besitzt Diagonallinien;
2. Ein Punkt, an dem im Setzabschnitt ein Stein entfernt wurde, ist bis zur Zugphase gesperrt;
3. Wird das Brett im Setzabschnitt vollständig gefüllt, verliert der erste Spieler;
4. In der Zugphase beginnt die Seite, die als Zweite gesetzt hat;
5. Mehrere Dreierreihen in einem Zug erlauben das Entfernen ebenso vieler Steine;
6. Die übrigen Regeln entsprechen im Wesentlichen Cheng San Qi.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="199"/>
        <source>1. 规则与成三棋基本相同，只是它的棋子有序号，
2. 相同序号、位置的“三连”不能重复提子；
3. 走棋阶段不能行动（被“闷”），则由对手继续走棋；
4. 一步出现几个“三连”就可以提几个子。</source>
        <translation>1. Die Regeln entsprechen im Wesentlichen Cheng San Qi, jedoch sind die Steine nummeriert;
2. Eine Dreierreihe mit gleichen Nummern an gleichen Positionen darf nicht erneut Steine entfernen;
3. Kann eine Seite in der Zugphase nicht ziehen, setzt der Gegner fort, statt zu gewinnen;
4. Mehrere Dreierreihen in einem Zug erlauben das Entfernen ebenso vieler Steine.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="204"/>
        <source>规则与成三棋基本相同，只是在走子阶段，当一方仅剩3子时，他可以飞子到任意空位。</source>
        <translation>Die Regeln entsprechen im Wesentlichen Cheng San Qi, jedoch darf eine Seite mit nur noch 3 Steinen in der Zugphase einen Stein auf einen beliebigen freien Punkt fliegen lassen.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="208"/>
        <source>未开局</source>
        <translation>Nicht begonnen</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="209"/>
        <source>玩家1</source>
        <translation>Spieler 1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="210"/>
        <source>玩家2</source>
        <translation>Spieler 2</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="211"/>
        <source>轮到%1落子，剩余%2子</source>
        <translation>%1 ist am Zug, noch %2 Steine zu setzen</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="212"/>
        <source>轮到%1去子，需去%2子</source>
        <translation>%1 ist am Zug, %2 Steine zu entfernen</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="213"/>
        <source>轮到%1选子移动</source>
        <translation>%1 ist am Zug, Stein zum Ziehen wählen</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="214"/>
        <source>轮到%1落子</source>
        <translation>%1 ist am Zug, Stein setzen</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="215"/>
        <source>平局。</source>
        <translation>Unentschieden.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="216"/>
        <source>恭喜玩家1获胜！</source>
        <translation>Glückwunsch, Spieler 1 gewinnt!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="217"/>
        <source>恭喜玩家2获胜！</source>
        <translation>Glückwunsch, Spieler 2 gewinnt!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="218"/>
        <source>玩家1认负，恭喜玩家2获胜！</source>
        <translation>Spieler 1 gibt auf. Glückwunsch, Spieler 2 gewinnt!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="219"/>
        <source>玩家2认负，恭喜玩家1获胜！</source>
        <translation>Spieler 2 gibt auf. Glückwunsch, Spieler 1 gewinnt!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="220"/>
        <source>摆满棋盘，恭喜玩家2获胜！</source>
        <translation>Das Brett ist voll. Glückwunsch, Spieler 2 gewinnt!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="221"/>
        <source>摆满棋盘，双方平局。</source>
        <translation>Das Brett ist voll. Unentschieden.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="222"/>
        <source>玩家1无子可走，恭喜玩家2获胜！</source>
        <translation>Spieler 1 kann nicht ziehen. Glückwunsch, Spieler 2 gewinnt!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="223"/>
        <source>玩家2无子可走，恭喜玩家1获胜！</source>
        <translation>Spieler 2 kann nicht ziehen. Glückwunsch, Spieler 1 gewinnt!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="224"/>
        <source>双方均无子可走，平局。</source>
        <translation>Beide Seiten können nicht ziehen. Unentschieden.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="225"/>
        <source>玩家1超时，恭喜玩家2获胜！</source>
        <translation>Zeitüberschreitung bei Spieler 1. Glückwunsch, Spieler 2 gewinnt!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="226"/>
        <source>玩家2超时，恭喜玩家1获胜！</source>
        <translation>Zeitüberschreitung bei Spieler 2. Glückwunsch, Spieler 1 gewinnt!</translation>
    </message>
</context>
<context>
    <name>NineChessWindow</name>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="274"/>
        <location filename="../src/ninechesswindow.cpp" line="476"/>
        <location filename="../src/ninechesswindow.cpp" line="1204"/>
        <source>九连棋 v%1</source>
        <translation>NineChess v%1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="425"/>
        <location filename="../src/ninechesswindow.cpp" line="495"/>
        <source>语言</source>
        <translation>Sprache</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 不限时</source>
        <translation> ∞ Min.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 不限步</source>
        <translation> ∞ Züge</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 限%1步</source>
        <translation> %1 Züge</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 限时%1分</source>
        <translation> %1 Min.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="721"/>
        <source>步数和时间限制</source>
        <translation>Zug- und Zeitbegrenzung</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="739"/>
        <source>超出限制步数判和：</source>
        <translation>Unentschieden bei Überschreiten des Zuglimits:</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="740"/>
        <source>任意一方超时判负：</source>
        <translation>Niederlage bei Zeitüberschreitung einer Seite:</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="741"/>
        <location filename="../src/ninechesswindow.cpp" line="745"/>
        <source>无限制</source>
        <translation>Unbegrenzt</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="742"/>
        <source>50步</source>
        <translation>50 Züge</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="743"/>
        <source>100步</source>
        <translation>100 Züge</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="744"/>
        <source>200步</source>
        <translation>200 Züge</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="746"/>
        <source>5分钟</source>
        <translation>5 Minuten</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="747"/>
        <source>10分钟</source>
        <translation>10 Minuten</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="748"/>
        <source>20分钟</source>
        <translation>20 Minuten</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="753"/>
        <location filename="../src/ninechesswindow.cpp" line="1139"/>
        <source>确定</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="754"/>
        <location filename="../src/ninechesswindow.cpp" line="1140"/>
        <source>取消</source>
        <translation>Abbrechen</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="827"/>
        <source>打开棋谱文件</source>
        <translation>Partieaufzeichnung öffnen</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="839"/>
        <source>文件过大</source>
        <translation>Datei zu groß</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="839"/>
        <source>不支持1MB以上文件</source>
        <translation>Dateien über 1 MB werden nicht unterstützt</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="879"/>
        <source>文件错误</source>
        <translation>Dateifehler</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="879"/>
        <source>不是正确的棋谱文件</source>
        <translation>Keine gültige Partieaufzeichnung</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="953"/>
        <source>棋谱.txt</source>
        <translation>Partie.txt</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="955"/>
        <source>保存棋谱文件</source>
        <translation>Partieaufzeichnung speichern</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1097"/>
        <source>AI设置</source>
        <translation>KI-Einstellungen</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1121"/>
        <source>玩家1 AI设置</source>
        <translation>KI-Einstellungen für Spieler 1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1122"/>
        <location filename="../src/ninechesswindow.cpp" line="1130"/>
        <source>深度</source>
        <translation>Tiefe</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1125"/>
        <location filename="../src/ninechesswindow.cpp" line="1133"/>
        <source>限时(秒)</source>
        <translation>Zeit (s)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1129"/>
        <source>玩家2 AI设置</source>
        <translation>KI-Einstellungen für Spieler 2</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1224"/>
        <source>NineChess v%1</source>
        <translation>NineChess v%1</translation>
    </message>
</context>
<context>
    <name>NineChessWindowClass</name>
    <message>
        <location filename="../ninechesswindow.ui" line="20"/>
        <source>九连棋</source>
        <translation>NineChess</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="71"/>
        <source>文件(&amp;F)</source>
        <translation>&amp;Datei</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="82"/>
        <source>棋局(&amp;C)</source>
        <translation>&amp;Partie</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="94"/>
        <source>招法(&amp;M)</source>
        <translation>&amp;Züge</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="107"/>
        <source>引擎(&amp;E)</source>
        <translation>&amp;Engine</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="118"/>
        <source>选项(&amp;O)</source>
        <translation>&amp;Optionen</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="130"/>
        <source>帮助(&amp;H)</source>
        <translation>&amp;Hilfe</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="138"/>
        <source>规则(&amp;R)</source>
        <translation>&amp;Regeln</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="154"/>
        <source>工具栏</source>
        <translation>Werkzeugleiste</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="214"/>
        <source>对战记录</source>
        <translation>Partieaufzeichnung</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="289"/>
        <source>玩家1</source>
        <translation>Spieler 1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="375"/>
        <source>玩家2</source>
        <translation>Spieler 2</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="452"/>
        <source>rule</source>
        <translation>rule</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="485"/>
        <source>新建(&amp;N)</source>
        <translation>&amp;Neu</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="491"/>
        <source>Ctrl+N</source>
        <translation>Ctrl+N</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="500"/>
        <source>打开(&amp;O)...</source>
        <translation>Ö&amp;ffnen...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="503"/>
        <source>Ctrl+O</source>
        <translation>Ctrl+O</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="512"/>
        <source>保存(&amp;S)</source>
        <translation>&amp;Speichern</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="515"/>
        <source>Ctrl+S</source>
        <translation>Ctrl+S</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="524"/>
        <source>另存为(&amp;A)...</source>
        <translation>Speichern &amp;unter...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="529"/>
        <source>退出(&amp;X)</source>
        <translation>Be&amp;enden</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="541"/>
        <source>编辑棋局(&amp;E)</source>
        <translation>Stellung &amp;bearbeiten</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="550"/>
        <source>上下翻转(&amp;F)</source>
        <translation>&amp;Vertikal spiegeln</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="559"/>
        <source>左右翻转(&amp;M)</source>
        <translation>&amp;Horizontal spiegeln</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="568"/>
        <source>顺时针旋转90°(&amp;R)</source>
        <translation>90° im &amp;Uhrzeigersinn drehen</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="571"/>
        <source>顺时针旋转90°(R)</source>
        <translation>90° im Uhrzeigersinn drehen (U)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="580"/>
        <source>逆时针旋转90°(&amp;L)</source>
        <translation>90° gegen den Uhrzeigersinn drehen (&amp;L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="583"/>
        <source>逆时针旋转90°(L)</source>
        <translation>90° gegen den Uhrzeigersinn drehen (L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="598"/>
        <source>黑白反转(&amp;B)</source>
        <translation>Farben &amp;tauschen</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="607"/>
        <source>初始局面(&amp;S)</source>
        <translation>&amp;Startstellung</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="610"/>
        <source>初始局面(S)</source>
        <translation>Startstellung (S)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="613"/>
        <source>Ctrl+Up</source>
        <translation>Ctrl+Up</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="622"/>
        <source>前一招(&amp;B)</source>
        <translation>&amp;Vorheriger Zug</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="625"/>
        <source>Ctrl+Left</source>
        <translation>Ctrl+Left</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="634"/>
        <source>后一招(&amp;F)</source>
        <translation>&amp;Nächster Zug</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="637"/>
        <source>后一招(F)</source>
        <translation>Nächster Zug (N)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="640"/>
        <source>Ctrl+Right</source>
        <translation>Ctrl+Right</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="649"/>
        <source>最后局面(&amp;E)</source>
        <translation>&amp;Endstellung</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="652"/>
        <source>Ctrl+Down</source>
        <translation>Ctrl+Down</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="664"/>
        <source>自动演示(&amp;A)</source>
        <translation>&amp;Demo abspielen</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="667"/>
        <source>自动演示(A)</source>
        <translation>Demo abspielen (D)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="676"/>
        <source>认输(&amp;G)</source>
        <translation>&amp;Aufgeben</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="684"/>
        <source>限制步数和时间(&amp;T)...</source>
        <translation>Zug- und &amp;Zeitbegrenzung...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="687"/>
        <source>限制步数和时间(T)</source>
        <translation>Zug- und Zeitbegrenzung (Z)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="702"/>
        <source>本机对战(&amp;L)</source>
        <translation>&amp;Lokales Spiel</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="705"/>
        <source>本机对战(L)</source>
        <translation>Lokales Spiel (L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="720"/>
        <source>网络对战(&amp;I)</source>
        <translation>&amp;Netzwerkspiel</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="723"/>
        <source>网络对战(I)</source>
        <translation>Netzwerkspiel (N)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="732"/>
        <source>引擎设置(&amp;E)...</source>
        <translation>&amp;Engine-Einstellungen...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="735"/>
        <source>引擎设置(E)</source>
        <translation>Engine-Einstellungen (E)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="750"/>
        <source>电脑执先手(&amp;T)</source>
        <translation>Computer &amp;beginnt</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="753"/>
        <source>电脑执先手(T)</source>
        <translation>Computer beginnt (B)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="765"/>
        <source>电脑执后手(&amp;R)</source>
        <translation>Computer spielt &amp;zweiten</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="768"/>
        <source>电脑执白(R)</source>
        <translation>Computer spielt Weiß (Z)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="777"/>
        <source>设置(&amp;O)</source>
        <translation>&amp;Einstellungen</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="788"/>
        <source>工具栏(&amp;T)</source>
        <translation>&amp;Werkzeugleiste</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="799"/>
        <source>信息栏(&amp;D)</source>
        <translation>&amp;Infobereich</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="810"/>
        <source>背景音乐(&amp;M)</source>
        <translation>Hintergrund&amp;musik</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="824"/>
        <source>落子音效(&amp;S)</source>
        <translation>Zug-&amp;Sound</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="835"/>
        <source>落子动画(&amp;A)</source>
        <translation>Zug&amp;animation</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="838"/>
        <source>落子动画(A)</source>
        <translation>Zuganimation (A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="847"/>
        <source>查看帮助(&amp;V)</source>
        <translation>&amp;Hilfe anzeigen</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="850"/>
        <source>F1</source>
        <translation>F1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="859"/>
        <source>作者主页(&amp;W)</source>
        <translation>&amp;Webseite des Autors</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="862"/>
        <source>作者主页(W)</source>
        <translation>Webseite des Autors (W)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="867"/>
        <source>关于(&amp;A)...</source>
        <translation>&amp;Über...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="870"/>
        <source>关于(A)</source>
        <translation>Über (Ü)</translation>
    </message>
</context>
</TS>
