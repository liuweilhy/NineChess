<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="es_ES">
<context>
    <name>GameController</name>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1170"/>
        <source>当前正在浏览历史局面。</source>
        <translation>Está viendo actualmente una posición anterior.</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1171"/>
        <source>是否在此局面下重新开始？悔棋者将承担时间损失！</source>
        <translation>¿Reiniciar desde esta posición? ¡Quien retroceda una jugada perderá tiempo!</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1174"/>
        <source>确定</source>
        <translation>Aceptar</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1175"/>
        <source>取消</source>
        <translation>Cancelar</translation>
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
        <translation>Da San Qi (12 piezas)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="180"/>
        <source>九连棋</source>
        <translation>NineChess</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="181"/>
        <source>莫里斯九子棋</source>
        <translation>Molino (Nine Men&apos;s Morris)</translation>
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
        <translation>1. Cada bando tiene 9 piezas y al inicio se colocan por turnos;
2. Alinear tres piezas permite retirar una pieza del rival;
3. No se puede retirar una pieza que forme parte de una línea de tres rival si hay otras disponibles;
4. Si se forman dos líneas de tres a la vez, solo se retira una pieza;
5. Tras colocar, se mueve por turnos un paso hacia un punto adyacente;
6. Se gana reduciendo al rival a menos de 3 piezas;
7. En la fase de movimiento, quedarse bloqueado (sin jugada posible) hace perder.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="192"/>
        <source>1. 双方各12颗子，棋盘有斜线；
2. 摆棋阶段被提子的位置不能再摆子，直到走棋阶段；
3. 摆棋阶段，摆满棋盘算先手负；
4. 走棋阶段，后摆棋的一方先走；
5. 一步出现几个“三连”就可以提几个子；
6. 其它规则与成三棋基本相同。</source>
        <translation>1. Cada bando tiene 12 piezas y el tablero incluye diagonales;
2. Un punto donde se retiró una pieza durante la colocación queda prohibido hasta la fase de movimiento;
3. Si se llena todo el tablero durante la colocación, pierde el primer jugador;
4. En la fase de movimiento empieza quien colocó en segundo lugar;
5. Varias líneas de tres en una jugada permiten retirar ese mismo número de piezas;
6. Las demás reglas son básicamente las del Cheng San Qi.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="199"/>
        <source>1. 规则与成三棋基本相同，只是它的棋子有序号，
2. 相同序号、位置的“三连”不能重复提子；
3. 走棋阶段不能行动（被“闷”），则由对手继续走棋；
4. 一步出现几个“三连”就可以提几个子。</source>
        <translation>1. Las reglas son básicamente las del Cheng San Qi, pero las piezas están numeradas;
2. Una línea de tres con los mismos números en las mismas posiciones no permite volver a retirar piezas;
3. En la fase de movimiento, quien no puede jugar deja que el rival continúe;
4. Varias líneas de tres en una jugada permiten retirar ese mismo número de piezas.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="204"/>
        <source>规则与成三棋基本相同，只是在走子阶段，当一方仅剩3子时，他可以飞子到任意空位。</source>
        <translation>Las reglas son básicamente las del Cheng San Qi, salvo que en la fase de movimiento, cuando a un bando solo le quedan 3 piezas, puede volar una a cualquier punto libre.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="208"/>
        <source>未开局</source>
        <translation>Sin comenzar</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="209"/>
        <source>玩家1</source>
        <translation>Jugador 1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="210"/>
        <source>玩家2</source>
        <translation>Jugador 2</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="211"/>
        <source>轮到%1落子，剩余%2子</source>
        <translation>Turno de %1 para colocar, quedan %2 piezas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="212"/>
        <source>轮到%1去子，需去%2子</source>
        <translation>Turno de %1 para retirar %2 piezas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="213"/>
        <source>轮到%1选子移动</source>
        <translation>Turno de %1 de elegir una pieza para mover</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="214"/>
        <source>轮到%1落子</source>
        <translation>Turno de %1 para colocar</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="215"/>
        <source>平局。</source>
        <translation>Empate.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="216"/>
        <source>恭喜玩家1获胜！</source>
        <translation>¡Felicidades, gana el jugador 1!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="217"/>
        <source>恭喜玩家2获胜！</source>
        <translation>¡Felicidades, gana el jugador 2!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="218"/>
        <source>玩家1认负，恭喜玩家2获胜！</source>
        <translation>El jugador 1 se rinde. ¡Felicidades, gana el jugador 2!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="219"/>
        <source>玩家2认负，恭喜玩家1获胜！</source>
        <translation>El jugador 2 se rinde. ¡Felicidades, gana el jugador 1!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="220"/>
        <source>摆满棋盘，恭喜玩家2获胜！</source>
        <translation>El tablero está lleno. ¡Felicidades, gana el jugador 2!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="221"/>
        <source>摆满棋盘，双方平局。</source>
        <translation>El tablero está lleno. Empate.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="222"/>
        <source>玩家1无子可走，恭喜玩家2获胜！</source>
        <translation>El jugador 1 no puede jugar. ¡Felicidades, gana el jugador 2!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="223"/>
        <source>玩家2无子可走，恭喜玩家1获胜！</source>
        <translation>El jugador 2 no puede jugar. ¡Felicidades, gana el jugador 1!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="224"/>
        <source>双方均无子可走，平局。</source>
        <translation>Ningún bando puede jugar. Empate.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="225"/>
        <source>玩家1超时，恭喜玩家2获胜！</source>
        <translation>El jugador 1 agotó su tiempo. ¡Felicidades, gana el jugador 2!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="226"/>
        <source>玩家2超时，恭喜玩家1获胜！</source>
        <translation>El jugador 2 agotó su tiempo. ¡Felicidades, gana el jugador 1!</translation>
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
        <translation>Idioma</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 不限时</source>
        <translation> ∞ min</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 不限步</source>
        <translation> ∞ jugadas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 限%1步</source>
        <translation> %1 jugadas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 限时%1分</source>
        <translation> %1 min</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="721"/>
        <source>步数和时间限制</source>
        <translation>Límites de jugadas y tiempo</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="739"/>
        <source>超出限制步数判和：</source>
        <translation>Tablas al superar el límite de jugadas:</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="740"/>
        <source>任意一方超时判负：</source>
        <translation>Derrota si cualquier bando agota su tiempo:</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="741"/>
        <location filename="../src/ninechesswindow.cpp" line="745"/>
        <source>无限制</source>
        <translation>Sin límite</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="742"/>
        <source>50步</source>
        <translation>50 jugadas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="743"/>
        <source>100步</source>
        <translation>100 jugadas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="744"/>
        <source>200步</source>
        <translation>200 jugadas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="746"/>
        <source>5分钟</source>
        <translation>5 minutos</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="747"/>
        <source>10分钟</source>
        <translation>10 minutos</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="748"/>
        <source>20分钟</source>
        <translation>20 minutos</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="753"/>
        <location filename="../src/ninechesswindow.cpp" line="1139"/>
        <source>确定</source>
        <translation>Aceptar</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="754"/>
        <location filename="../src/ninechesswindow.cpp" line="1140"/>
        <source>取消</source>
        <translation>Cancelar</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="827"/>
        <source>打开棋谱文件</source>
        <translation>Abrir registro de partida</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="839"/>
        <source>文件过大</source>
        <translation>Archivo demasiado grande</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="839"/>
        <source>不支持1MB以上文件</source>
        <translation>No se admiten archivos de más de 1 MB</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="879"/>
        <source>文件错误</source>
        <translation>Error de archivo</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="879"/>
        <source>不是正确的棋谱文件</source>
        <translation>No es un archivo de partida válido</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="953"/>
        <source>棋谱.txt</source>
        <translation>Partida.txt</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="955"/>
        <source>保存棋谱文件</source>
        <translation>Guardar registro de partida</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1097"/>
        <source>AI设置</source>
        <translation>Ajustes de IA</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1121"/>
        <source>玩家1 AI设置</source>
        <translation>Ajustes de IA del jugador 1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1122"/>
        <location filename="../src/ninechesswindow.cpp" line="1130"/>
        <source>深度</source>
        <translation>Profundidad</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1125"/>
        <location filename="../src/ninechesswindow.cpp" line="1133"/>
        <source>限时(秒)</source>
        <translation>Tiempo (s)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1129"/>
        <source>玩家2 AI设置</source>
        <translation>Ajustes de IA del jugador 2</translation>
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
        <translation>&amp;Archivo</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="82"/>
        <source>棋局(&amp;C)</source>
        <translation>&amp;Partida</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="94"/>
        <source>招法(&amp;M)</source>
        <translation>&amp;Jugadas</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="107"/>
        <source>引擎(&amp;E)</source>
        <translation>&amp;Motor</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="118"/>
        <source>选项(&amp;O)</source>
        <translation>&amp;Opciones</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="130"/>
        <source>帮助(&amp;H)</source>
        <translation>A&amp;yuda</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="138"/>
        <source>规则(&amp;R)</source>
        <translation>&amp;Reglas</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="154"/>
        <source>工具栏</source>
        <translation>Barra de herramientas</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="214"/>
        <source>对战记录</source>
        <translation>Registro de la partida</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="289"/>
        <source>玩家1</source>
        <translation>Jugador 1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="375"/>
        <source>玩家2</source>
        <translation>Jugador 2</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="452"/>
        <source>rule</source>
        <translation>rule</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="485"/>
        <source>新建(&amp;N)</source>
        <translation>&amp;Nuevo</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="491"/>
        <source>Ctrl+N</source>
        <translation>Ctrl+N</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="500"/>
        <source>打开(&amp;O)...</source>
        <translation>A&amp;brir...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="503"/>
        <source>Ctrl+O</source>
        <translation>Ctrl+O</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="512"/>
        <source>保存(&amp;S)</source>
        <translation>&amp;Guardar</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="515"/>
        <source>Ctrl+S</source>
        <translation>Ctrl+S</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="524"/>
        <source>另存为(&amp;A)...</source>
        <translation>Guardar &amp;como...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="529"/>
        <source>退出(&amp;X)</source>
        <translation>&amp;Salir</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="541"/>
        <source>编辑棋局(&amp;E)</source>
        <translation>&amp;Editar posición</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="550"/>
        <source>上下翻转(&amp;F)</source>
        <translation>Voltear &amp;verticalmente</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="559"/>
        <source>左右翻转(&amp;M)</source>
        <translation>Voltear &amp;horizontalmente</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="568"/>
        <source>顺时针旋转90°(&amp;R)</source>
        <translation>&amp;Girar 90° en sentido horario</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="571"/>
        <source>顺时针旋转90°(R)</source>
        <translation>Girar 90° en sentido horario (G)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="580"/>
        <source>逆时针旋转90°(&amp;L)</source>
        <translation>Girar 90° en sentido &amp;antihorario</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="583"/>
        <source>逆时针旋转90°(L)</source>
        <translation>Girar 90° en sentido antihorario (A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="598"/>
        <source>黑白反转(&amp;B)</source>
        <translation>Invertir &amp;colores</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="607"/>
        <source>初始局面(&amp;S)</source>
        <translation>Posición &amp;inicial</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="610"/>
        <source>初始局面(S)</source>
        <translation>Posición inicial (I)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="613"/>
        <source>Ctrl+Up</source>
        <translation>Ctrl+Up</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="622"/>
        <source>前一招(&amp;B)</source>
        <translation>Jugada &amp;anterior</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="625"/>
        <source>Ctrl+Left</source>
        <translation>Ctrl+Left</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="634"/>
        <source>后一招(&amp;F)</source>
        <translation>Jugada si&amp;guiente</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="637"/>
        <source>后一招(F)</source>
        <translation>Jugada siguiente (G)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="640"/>
        <source>Ctrl+Right</source>
        <translation>Ctrl+Right</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="649"/>
        <source>最后局面(&amp;E)</source>
        <translation>Posición &amp;final</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="652"/>
        <source>Ctrl+Down</source>
        <translation>Ctrl+Down</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="664"/>
        <source>自动演示(&amp;A)</source>
        <translation>&amp;Reproducción automática</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="667"/>
        <source>自动演示(A)</source>
        <translation>Reproducción automática (R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="676"/>
        <source>认输(&amp;G)</source>
        <translation>Ren&amp;dirse</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="684"/>
        <source>限制步数和时间(&amp;T)...</source>
        <translation>Límites de jugadas y &amp;tiempo...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="687"/>
        <source>限制步数和时间(T)</source>
        <translation>Límites de jugadas y tiempo (T)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="702"/>
        <source>本机对战(&amp;L)</source>
        <translation>Partida &amp;local</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="705"/>
        <source>本机对战(L)</source>
        <translation>Partida local (L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="720"/>
        <source>网络对战(&amp;I)</source>
        <translation>Partida en &amp;red</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="723"/>
        <source>网络对战(I)</source>
        <translation>Partida en red (R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="732"/>
        <source>引擎设置(&amp;E)...</source>
        <translation>Ajustes del &amp;motor...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="735"/>
        <source>引擎设置(E)</source>
        <translation>Ajustes del motor (M)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="750"/>
        <source>电脑执先手(&amp;T)</source>
        <translation>La computadora juega &amp;primero</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="753"/>
        <source>电脑执先手(T)</source>
        <translation>La computadora juega primero (P)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="765"/>
        <source>电脑执后手(&amp;R)</source>
        <translation>La computadora juega en se&amp;gundo lugar</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="768"/>
        <source>电脑执白(R)</source>
        <translation>La computadora juega con blancas (G)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="777"/>
        <source>设置(&amp;O)</source>
        <translation>A&amp;justes</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="788"/>
        <source>工具栏(&amp;T)</source>
        <translation>Barra de &amp;herramientas</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="799"/>
        <source>信息栏(&amp;D)</source>
        <translation>Panel de información (&amp;D)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="810"/>
        <source>背景音乐(&amp;M)</source>
        <translation>Música de fondo (&amp;M)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="824"/>
        <source>落子音效(&amp;S)</source>
        <translation>&amp;Efectos de sonido</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="835"/>
        <source>落子动画(&amp;A)</source>
        <translation>&amp;Animación de jugadas</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="838"/>
        <source>落子动画(A)</source>
        <translation>Animación de jugadas (A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="847"/>
        <source>查看帮助(&amp;V)</source>
        <translation>&amp;Ver ayuda</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="850"/>
        <source>F1</source>
        <translation>F1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="859"/>
        <source>作者主页(&amp;W)</source>
        <translation>Página del autor (&amp;W)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="862"/>
        <source>作者主页(W)</source>
        <translation>Página del autor (W)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="867"/>
        <source>关于(&amp;A)...</source>
        <translation>&amp;Acerca de...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="870"/>
        <source>关于(A)</source>
        <translation>Acerca de (A)</translation>
    </message>
</context>
</TS>
