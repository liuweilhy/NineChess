<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="fr_FR">
<context>
    <name>GameController</name>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1170"/>
        <source>当前正在浏览历史局面。</source>
        <translation>Vous consultez actuellement une position antérieure.</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1171"/>
        <source>是否在此局面下重新开始？悔棋者将承担时间损失！</source>
        <translation>Recommencer depuis cette position ? Celui qui annule un coup perdra du temps !</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1174"/>
        <source>确定</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1175"/>
        <source>取消</source>
        <translation>Annuler</translation>
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
        <translation>Da San Qi (12 pièces)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="180"/>
        <source>九连棋</source>
        <translation>NineChess</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="181"/>
        <source>莫里斯九子棋</source>
        <translation>Moulin (Nine Men&apos;s Morris)</translation>
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
        <translation>1. Chaque camp dispose de 9 pions, placés à tour de rôle au début de la partie ;
2. Aligner trois pions permet de retirer un pion adverse ;
3. Un pion faisant partie d&apos;un alignement adverse ne peut être retiré que si aucun autre pion n&apos;est disponible ;
4. Si deux alignements apparaissent en même temps, un seul pion peut être retiré ;
5. Une fois le placement terminé, on déplace à tour de rôle un pion d&apos;un pas vers un point adjacent ;
6. La victoire est acquise en réduisant l&apos;adversaire à moins de 3 pions ;
7. En phase de déplacement, être bloqué (incapable de jouer) fait perdre la partie.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="192"/>
        <source>1. 双方各12颗子，棋盘有斜线；
2. 摆棋阶段被提子的位置不能再摆子，直到走棋阶段；
3. 摆棋阶段，摆满棋盘算先手负；
4. 走棋阶段，后摆棋的一方先走；
5. 一步出现几个“三连”就可以提几个子；
6. 其它规则与成三棋基本相同。</source>
        <translation>1. Chaque camp dispose de 12 pions et le plateau comporte des diagonales ;
2. Un point où un pion a été retiré pendant le placement reste interdit jusqu&apos;à la phase de déplacement ;
3. Si le plateau est entièrement rempli pendant le placement, le premier joueur perd ;
4. En phase de déplacement, le camp qui a placé en second commence ;
5. Plusieurs alignements dans un même coup permettent de retirer autant de pions ;
6. Les autres règles sont essentiellement celles du Cheng San Qi.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="199"/>
        <source>1. 规则与成三棋基本相同，只是它的棋子有序号，
2. 相同序号、位置的“三连”不能重复提子；
3. 走棋阶段不能行动（被“闷”），则由对手继续走棋；
4. 一步出现几个“三连”就可以提几个子。</source>
        <translation>1. Les règles sont essentiellement celles du Cheng San Qi, mais les pions sont numérotés ;
2. Un alignement de mêmes numéros aux mêmes positions ne peut plus retirer de pion ;
3. En phase de déplacement, un camp bloqué laisse simplement l&apos;adversaire continuer ;
4. Plusieurs alignements dans un même coup permettent de retirer autant de pions.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="204"/>
        <source>规则与成三棋基本相同，只是在走子阶段，当一方仅剩3子时，他可以飞子到任意空位。</source>
        <translation>Les règles sont essentiellement celles du Cheng San Qi, sauf qu&apos;en phase de déplacement, un camp réduit à 3 pions peut en faire voler un vers n&apos;importe quel point libre.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="208"/>
        <source>未开局</source>
        <translation>Partie non commencée</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="209"/>
        <source>玩家1</source>
        <translation>Joueur 1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="210"/>
        <source>玩家2</source>
        <translation>Joueur 2</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="211"/>
        <source>轮到%1落子，剩余%2子</source>
        <translation>Au tour de %1 de placer, %2 pions restants</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="212"/>
        <source>轮到%1去子，需去%2子</source>
        <translation>Au tour de %1 de retirer %2 pions</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="213"/>
        <source>轮到%1选子移动</source>
        <translation>Au tour de %1 de choisir un pion à déplacer</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="214"/>
        <source>轮到%1落子</source>
        <translation>Au tour de %1 de placer</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="215"/>
        <source>平局。</source>
        <translation>Match nul.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="216"/>
        <source>恭喜玩家1获胜！</source>
        <translation>Félicitations, le joueur 1 gagne !</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="217"/>
        <source>恭喜玩家2获胜！</source>
        <translation>Félicitations, le joueur 2 gagne !</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="218"/>
        <source>玩家1认负，恭喜玩家2获胜！</source>
        <translation>Le joueur 1 abandonne. Félicitations, le joueur 2 gagne !</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="219"/>
        <source>玩家2认负，恭喜玩家1获胜！</source>
        <translation>Le joueur 2 abandonne. Félicitations, le joueur 1 gagne !</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="220"/>
        <source>摆满棋盘，恭喜玩家2获胜！</source>
        <translation>Le plateau est plein. Félicitations, le joueur 2 gagne !</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="221"/>
        <source>摆满棋盘，双方平局。</source>
        <translation>Le plateau est plein. Match nul.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="222"/>
        <source>玩家1无子可走，恭喜玩家2获胜！</source>
        <translation>Le joueur 1 ne peut plus jouer. Félicitations, le joueur 2 gagne !</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="223"/>
        <source>玩家2无子可走，恭喜玩家1获胜！</source>
        <translation>Le joueur 2 ne peut plus jouer. Félicitations, le joueur 1 gagne !</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="224"/>
        <source>双方均无子可走，平局。</source>
        <translation>Aucun camp ne peut jouer. Match nul.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="225"/>
        <source>玩家1超时，恭喜玩家2获胜！</source>
        <translation>Le joueur 1 a dépassé le temps. Félicitations, le joueur 2 gagne !</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="226"/>
        <source>玩家2超时，恭喜玩家1获胜！</source>
        <translation>Le joueur 2 a dépassé le temps. Félicitations, le joueur 1 gagne !</translation>
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
        <translation>Langue</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 不限时</source>
        <translation> ∞ min</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 不限步</source>
        <translation> ∞ coups</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 限%1步</source>
        <translation> %1 coups</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 限时%1分</source>
        <translation> %1 min</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="721"/>
        <source>步数和时间限制</source>
        <translation>Limites de coups et de temps</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="739"/>
        <source>超出限制步数判和：</source>
        <translation>Match nul au-delà de la limite de coups :</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="740"/>
        <source>任意一方超时判负：</source>
        <translation>Défaite en cas de temps écoulé pour un camp :</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="741"/>
        <location filename="../src/ninechesswindow.cpp" line="745"/>
        <source>无限制</source>
        <translation>Illimité</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="742"/>
        <source>50步</source>
        <translation>50 coups</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="743"/>
        <source>100步</source>
        <translation>100 coups</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="744"/>
        <source>200步</source>
        <translation>200 coups</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="746"/>
        <source>5分钟</source>
        <translation>5 minutes</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="747"/>
        <source>10分钟</source>
        <translation>10 minutes</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="748"/>
        <source>20分钟</source>
        <translation>20 minutes</translation>
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
        <translation>Annuler</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="827"/>
        <source>打开棋谱文件</source>
        <translation>Ouvrir une partie enregistrée</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="839"/>
        <source>文件过大</source>
        <translation>Fichier trop volumineux</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="839"/>
        <source>不支持1MB以上文件</source>
        <translation>Les fichiers de plus de 1 Mo ne sont pas pris en charge</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="879"/>
        <source>文件错误</source>
        <translation>Erreur de fichier</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="879"/>
        <source>不是正确的棋谱文件</source>
        <translation>Ce n&apos;est pas un fichier de partie valide</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="953"/>
        <source>棋谱.txt</source>
        <translation>Partie.txt</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="955"/>
        <source>保存棋谱文件</source>
        <translation>Enregistrer la partie</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1097"/>
        <source>AI设置</source>
        <translation>Réglages de l&apos;IA</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1121"/>
        <source>玩家1 AI设置</source>
        <translation>Réglages IA du joueur 1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1122"/>
        <location filename="../src/ninechesswindow.cpp" line="1130"/>
        <source>深度</source>
        <translation>Profondeur</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1125"/>
        <location filename="../src/ninechesswindow.cpp" line="1133"/>
        <source>限时(秒)</source>
        <translation>Temps (s)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1129"/>
        <source>玩家2 AI设置</source>
        <translation>Réglages IA du joueur 2</translation>
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
        <translation>&amp;Fichier</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="82"/>
        <source>棋局(&amp;C)</source>
        <translation>&amp;Partie</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="94"/>
        <source>招法(&amp;M)</source>
        <translation>&amp;Coups</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="107"/>
        <source>引擎(&amp;E)</source>
        <translation>&amp;Moteur</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="118"/>
        <source>选项(&amp;O)</source>
        <translation>&amp;Options</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="130"/>
        <source>帮助(&amp;H)</source>
        <translation>&amp;Aide</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="138"/>
        <source>规则(&amp;R)</source>
        <translation>&amp;Règles</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="154"/>
        <source>工具栏</source>
        <translation>Barre d&apos;outils</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="214"/>
        <source>对战记录</source>
        <translation>Historique de la partie</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="289"/>
        <source>玩家1</source>
        <translation>Joueur 1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="375"/>
        <source>玩家2</source>
        <translation>Joueur 2</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="452"/>
        <source>rule</source>
        <translation>rule</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="485"/>
        <source>新建(&amp;N)</source>
        <translation>&amp;Nouveau</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="491"/>
        <source>Ctrl+N</source>
        <translation>Ctrl+N</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="500"/>
        <source>打开(&amp;O)...</source>
        <translation>&amp;Ouvrir...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="503"/>
        <source>Ctrl+O</source>
        <translation>Ctrl+O</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="512"/>
        <source>保存(&amp;S)</source>
        <translation>&amp;Enregistrer</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="515"/>
        <source>Ctrl+S</source>
        <translation>Ctrl+S</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="524"/>
        <source>另存为(&amp;A)...</source>
        <translation>Enregistrer &amp;sous...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="529"/>
        <source>退出(&amp;X)</source>
        <translation>&amp;Quitter</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="541"/>
        <source>编辑棋局(&amp;E)</source>
        <translation>&amp;Éditer la position</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="550"/>
        <source>上下翻转(&amp;F)</source>
        <translation>Retourner &amp;verticalement</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="559"/>
        <source>左右翻转(&amp;M)</source>
        <translation>Retourner &amp;horizontalement</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="568"/>
        <source>顺时针旋转90°(&amp;R)</source>
        <translation>Pivoter de 90° dans le sens horaire (&amp;R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="571"/>
        <source>顺时针旋转90°(R)</source>
        <translation>Pivoter de 90° dans le sens horaire (R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="580"/>
        <source>逆时针旋转90°(&amp;L)</source>
        <translation>Pivoter de 90° dans le sens antihoraire (&amp;L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="583"/>
        <source>逆时针旋转90°(L)</source>
        <translation>Pivoter de 90° dans le sens antihoraire (L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="598"/>
        <source>黑白反转(&amp;B)</source>
        <translation>Inverser les &amp;couleurs</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="607"/>
        <source>初始局面(&amp;S)</source>
        <translation>Position &amp;initiale</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="610"/>
        <source>初始局面(S)</source>
        <translation>Position initiale (I)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="613"/>
        <source>Ctrl+Up</source>
        <translation>Ctrl+Up</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="622"/>
        <source>前一招(&amp;B)</source>
        <translation>&amp;Coup précédent</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="625"/>
        <source>Ctrl+Left</source>
        <translation>Ctrl+Left</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="634"/>
        <source>后一招(&amp;F)</source>
        <translation>Coup &amp;suivant</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="637"/>
        <source>后一招(F)</source>
        <translation>Coup suivant (S)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="640"/>
        <source>Ctrl+Right</source>
        <translation>Ctrl+Right</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="649"/>
        <source>最后局面(&amp;E)</source>
        <translation>Position &amp;finale</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="652"/>
        <source>Ctrl+Down</source>
        <translation>Ctrl+Down</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="664"/>
        <source>自动演示(&amp;A)</source>
        <translation>&amp;Lecture automatique</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="667"/>
        <source>自动演示(A)</source>
        <translation>Lecture automatique (L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="676"/>
        <source>认输(&amp;G)</source>
        <translation>&amp;Abandonner</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="684"/>
        <source>限制步数和时间(&amp;T)...</source>
        <translation>Limites de coups et de &amp;temps...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="687"/>
        <source>限制步数和时间(T)</source>
        <translation>Limites de coups et de temps (T)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="702"/>
        <source>本机对战(&amp;L)</source>
        <translation>Partie &amp;locale</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="705"/>
        <source>本机对战(L)</source>
        <translation>Partie locale (L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="720"/>
        <source>网络对战(&amp;I)</source>
        <translation>Partie en &amp;réseau</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="723"/>
        <source>网络对战(I)</source>
        <translation>Partie en réseau (R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="732"/>
        <source>引擎设置(&amp;E)...</source>
        <translation>Réglages du &amp;moteur...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="735"/>
        <source>引擎设置(E)</source>
        <translation>Réglages du moteur (M)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="750"/>
        <source>电脑执先手(&amp;T)</source>
        <translation>L&apos;ordinateur joue en &amp;premier</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="753"/>
        <source>电脑执先手(T)</source>
        <translation>L&apos;ordinateur joue en premier (P)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="765"/>
        <source>电脑执后手(&amp;R)</source>
        <translation>L&apos;ordinateur joue en &amp;second</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="768"/>
        <source>电脑执白(R)</source>
        <translation>L&apos;ordinateur joue les blancs (S)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="777"/>
        <source>设置(&amp;O)</source>
        <translation>&amp;Paramètres</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="788"/>
        <source>工具栏(&amp;T)</source>
        <translation>Barre d&apos;&amp;outils</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="799"/>
        <source>信息栏(&amp;D)</source>
        <translation>Panneau d&apos;informations (&amp;D)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="810"/>
        <source>背景音乐(&amp;M)</source>
        <translation>Musique de fond (&amp;M)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="824"/>
        <source>落子音效(&amp;S)</source>
        <translation>Son des &amp;coups</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="835"/>
        <source>落子动画(&amp;A)</source>
        <translation>&amp;Animation des coups</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="838"/>
        <source>落子动画(A)</source>
        <translation>Animation des coups (A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="847"/>
        <source>查看帮助(&amp;V)</source>
        <translation>&amp;Voir l&apos;aide</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="850"/>
        <source>F1</source>
        <translation>F1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="859"/>
        <source>作者主页(&amp;W)</source>
        <translation>Page d&apos;accueil de l&apos;&amp;auteur</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="862"/>
        <source>作者主页(W)</source>
        <translation>Page d&apos;accueil de l&apos;auteur (A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="867"/>
        <source>关于(&amp;A)...</source>
        <translation>À &amp;propos...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="870"/>
        <source>关于(A)</source>
        <translation>À propos (P)</translation>
    </message>
</context>
</TS>
