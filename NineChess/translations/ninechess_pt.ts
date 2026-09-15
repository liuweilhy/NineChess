<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="pt_BR">
<context>
    <name>GameController</name>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1170"/>
        <source>当前正在浏览历史局面。</source>
        <translation>Você está visualizando uma posição anterior.</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1171"/>
        <source>是否在此局面下重新开始？悔棋者将承担时间损失！</source>
        <translation>Reiniciar a partir desta posição? Quem voltar uma jogada perderá tempo!</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1174"/>
        <source>确定</source>
        <translation>OK</translation>
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
        <translation>Da San Qi (12 peças)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="180"/>
        <source>九连棋</source>
        <translation>NineChess</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="181"/>
        <source>莫里斯九子棋</source>
        <translation>Moinho (Nine Men&apos;s Morris)</translation>
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
        <translation>1. Cada lado tem 9 peças e, no início, elas são colocadas alternadamente;
2. Alinhar três peças permite remover uma peça do adversário;
3. Não se pode remover uma peça que faça parte de uma linha de três adversária se houver outras disponíveis;
4. Se duas linhas de três se formarem ao mesmo tempo, apenas uma peça pode ser removida;
5. Após a colocação, move-se alternadamente um passo para um ponto adjacente;
6. Vence quem reduzir o adversário a menos de 3 peças;
7. Na fase de movimento, ficar bloqueado (sem jogada possível) significa derrota.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="192"/>
        <source>1. 双方各12颗子，棋盘有斜线；
2. 摆棋阶段被提子的位置不能再摆子，直到走棋阶段；
3. 摆棋阶段，摆满棋盘算先手负；
4. 走棋阶段，后摆棋的一方先走；
5. 一步出现几个“三连”就可以提几个子；
6. 其它规则与成三棋基本相同。</source>
        <translation>1. Cada lado tem 12 peças e o tabuleiro possui diagonais;
2. Um ponto onde uma peça foi removida durante a colocação fica proibido até a fase de movimento;
3. Se o tabuleiro for totalmente preenchido na colocação, o primeiro jogador perde;
4. Na fase de movimento, começa quem colocou por segundo;
5. Várias linhas de três em uma jogada permitem remover esse mesmo número de peças;
6. As demais regras são basicamente as do Cheng San Qi.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="199"/>
        <source>1. 规则与成三棋基本相同，只是它的棋子有序号，
2. 相同序号、位置的“三连”不能重复提子；
3. 走棋阶段不能行动（被“闷”），则由对手继续走棋；
4. 一步出现几个“三连”就可以提几个子。</source>
        <translation>1. As regras são basicamente as do Cheng San Qi, mas as peças são numeradas;
2. Uma linha de três com os mesmos números nas mesmas posições não permite remover peças novamente;
3. Na fase de movimento, quem não pode jogar deixa o adversário continuar;
4. Várias linhas de três em uma jogada permitem remover esse mesmo número de peças.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="204"/>
        <source>规则与成三棋基本相同，只是在走子阶段，当一方仅剩3子时，他可以飞子到任意空位。</source>
        <translation>As regras são basicamente as do Cheng San Qi, exceto que, na fase de movimento, um lado com apenas 3 peças pode voar uma peça para qualquer ponto vazio.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="208"/>
        <source>未开局</source>
        <translation>Não iniciado</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="209"/>
        <source>玩家1</source>
        <translation>Jogador 1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="210"/>
        <source>玩家2</source>
        <translation>Jogador 2</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="211"/>
        <source>轮到%1落子，剩余%2子</source>
        <translation>Vez de %1 colocar, restam %2 peças</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="212"/>
        <source>轮到%1去子，需去%2子</source>
        <translation>Vez de %1 remover %2 peças</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="213"/>
        <source>轮到%1选子移动</source>
        <translation>Vez de %1 escolher uma peça para mover</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="214"/>
        <source>轮到%1落子</source>
        <translation>Vez de %1 colocar</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="215"/>
        <source>平局。</source>
        <translation>Empate.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="216"/>
        <source>恭喜玩家1获胜！</source>
        <translation>Parabéns, o jogador 1 venceu!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="217"/>
        <source>恭喜玩家2获胜！</source>
        <translation>Parabéns, o jogador 2 venceu!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="218"/>
        <source>玩家1认负，恭喜玩家2获胜！</source>
        <translation>O jogador 1 desistiu. Parabéns, o jogador 2 venceu!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="219"/>
        <source>玩家2认负，恭喜玩家1获胜！</source>
        <translation>O jogador 2 desistiu. Parabéns, o jogador 1 venceu!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="220"/>
        <source>摆满棋盘，恭喜玩家2获胜！</source>
        <translation>O tabuleiro está cheio. Parabéns, o jogador 2 venceu!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="221"/>
        <source>摆满棋盘，双方平局。</source>
        <translation>O tabuleiro está cheio. Empate.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="222"/>
        <source>玩家1无子可走，恭喜玩家2获胜！</source>
        <translation>O jogador 1 não pode jogar. Parabéns, o jogador 2 venceu!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="223"/>
        <source>玩家2无子可走，恭喜玩家1获胜！</source>
        <translation>O jogador 2 não pode jogar. Parabéns, o jogador 1 venceu!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="224"/>
        <source>双方均无子可走，平局。</source>
        <translation>Nenhum lado pode jogar. Empate.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="225"/>
        <source>玩家1超时，恭喜玩家2获胜！</source>
        <translation>O jogador 1 esgotou o tempo. Parabéns, o jogador 2 venceu!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="226"/>
        <source>玩家2超时，恭喜玩家1获胜！</source>
        <translation>O jogador 2 esgotou o tempo. Parabéns, o jogador 1 venceu!</translation>
    </message>
</context>
<context>
    <name>NineChessWindow</name>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="274"/>
        <location filename="../src/ninechesswindow.cpp" line="476"/>
        <location filename="../src/ninechesswindow.cpp" line="1208"/>
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
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 不限时</source>
        <translation> Sem limite de tempo</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 不限步</source>
        <translation> Sem limite de jogadas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="698"/>
        <source> 限%1步</source>
        <translation> Máx. %1 jogadas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="700"/>
        <source> 限时%1分</source>
        <translation> Limite %1 min</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="725"/>
        <source>步数和时间限制</source>
        <translation>Limites de jogadas e tempo</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="743"/>
        <source>超出限制步数判和：</source>
        <translation>Empate ao exceder o limite de jogadas:</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="744"/>
        <source>任意一方超时判负：</source>
        <translation>Derrota se qualquer lado esgotar o tempo:</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="745"/>
        <location filename="../src/ninechesswindow.cpp" line="749"/>
        <source>无限制</source>
        <translation>Ilimitado</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="746"/>
        <source>50步</source>
        <translation>50 jogadas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="747"/>
        <source>100步</source>
        <translation>100 jogadas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="748"/>
        <source>200步</source>
        <translation>200 jogadas</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="750"/>
        <source>5分钟</source>
        <translation>5 minutos</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="751"/>
        <source>10分钟</source>
        <translation>10 minutos</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="752"/>
        <source>20分钟</source>
        <translation>20 minutos</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="757"/>
        <location filename="../src/ninechesswindow.cpp" line="1143"/>
        <source>确定</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="758"/>
        <location filename="../src/ninechesswindow.cpp" line="1144"/>
        <source>取消</source>
        <translation>Cancelar</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="831"/>
        <source>打开棋谱文件</source>
        <translation>Abrir registro de partida</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="843"/>
        <source>文件过大</source>
        <translation>Arquivo muito grande</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="843"/>
        <source>不支持1MB以上文件</source>
        <translation>Arquivos com mais de 1 MB não são suportados</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="883"/>
        <source>文件错误</source>
        <translation>Erro de arquivo</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="883"/>
        <source>不是正确的棋谱文件</source>
        <translation>Não é um arquivo de partida válido</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="957"/>
        <source>棋谱.txt</source>
        <translation>棋谱.txt</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="959"/>
        <source>保存棋谱文件</source>
        <translation>Salvar registro de partida</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1101"/>
        <source>AI设置</source>
        <translation>Configurações de IA</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1125"/>
        <source>玩家1 AI设置</source>
        <translation>Configurações de IA do jogador 1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1126"/>
        <location filename="../src/ninechesswindow.cpp" line="1134"/>
        <source>深度</source>
        <translation>Profundidade</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1129"/>
        <location filename="../src/ninechesswindow.cpp" line="1137"/>
        <source>限时(秒)</source>
        <translation>Tempo (s)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1133"/>
        <source>玩家2 AI设置</source>
        <translation>Configurações de IA do jogador 2</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1228"/>
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
        <translation>&amp;Arquivo</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="82"/>
        <source>棋局(&amp;C)</source>
        <translation>&amp;Partida</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="94"/>
        <source>招法(&amp;M)</source>
        <translation>&amp;Jogadas</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="107"/>
        <source>引擎(&amp;E)</source>
        <translation>&amp;Motor</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="118"/>
        <source>选项(&amp;O)</source>
        <translation>&amp;Opções</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="130"/>
        <source>帮助(&amp;H)</source>
        <translation>A&amp;juda</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="138"/>
        <source>规则(&amp;R)</source>
        <translation>&amp;Regras</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="154"/>
        <source>工具栏</source>
        <translation>Barra de ferramentas</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="214"/>
        <source>对战记录</source>
        <translation>Registro da partida</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="289"/>
        <source>玩家1</source>
        <translation>Jogador 1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="375"/>
        <source>玩家2</source>
        <translation>Jogador 2</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="452"/>
        <source>rule</source>
        <translation>rule</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="485"/>
        <source>新建(&amp;N)</source>
        <translation>&amp;Novo</translation>
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
        <translation>&amp;Salvar</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="515"/>
        <source>Ctrl+S</source>
        <translation>Ctrl+S</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="524"/>
        <source>另存为(&amp;A)...</source>
        <translation>Salvar &amp;como...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="529"/>
        <source>退出(&amp;X)</source>
        <translation>Sai&amp;r</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="541"/>
        <source>编辑棋局(&amp;E)</source>
        <translation>&amp;Editar posição</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="550"/>
        <source>上下翻转(&amp;F)</source>
        <translation>Virar &amp;verticalmente</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="559"/>
        <source>左右翻转(&amp;M)</source>
        <translation>Virar &amp;horizontalmente</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="568"/>
        <source>顺时针旋转90°(&amp;R)</source>
        <translation>Girar 90° no sentido &amp;horário</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="571"/>
        <source>顺时针旋转90°(R)</source>
        <translation>Girar 90° no sentido horário (R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="580"/>
        <source>逆时针旋转90°(&amp;L)</source>
        <translation>Girar 90° no sentido &amp;anti-horário</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="583"/>
        <source>逆时针旋转90°(L)</source>
        <translation>Girar 90° no sentido anti-horário (L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="598"/>
        <source>黑白反转(&amp;B)</source>
        <translation>Inverter &amp;cores</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="607"/>
        <source>初始局面(&amp;S)</source>
        <translation>Posição &amp;inicial</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="610"/>
        <source>初始局面(S)</source>
        <translation>Posição inicial (S)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="613"/>
        <source>Ctrl+Up</source>
        <translation>Ctrl+Up</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="622"/>
        <source>前一招(&amp;B)</source>
        <translation>Jogada &amp;anterior</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="625"/>
        <source>Ctrl+Left</source>
        <translation>Ctrl+Left</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="634"/>
        <source>后一招(&amp;F)</source>
        <translation>Jogada se&amp;guinte</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="637"/>
        <source>后一招(F)</source>
        <translation>Jogada seguinte (F)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="640"/>
        <source>Ctrl+Right</source>
        <translation>Ctrl+Right</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="649"/>
        <source>最后局面(&amp;E)</source>
        <translation>Posição &amp;final</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="652"/>
        <source>Ctrl+Down</source>
        <translation>Ctrl+Down</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="664"/>
        <source>自动演示(&amp;A)</source>
        <translation>Reprodução &amp;automática</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="667"/>
        <source>自动演示(A)</source>
        <translation>Reprodução automática (A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="676"/>
        <source>认输(&amp;G)</source>
        <translation>Des&amp;istir</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="684"/>
        <source>限制步数和时间(&amp;T)...</source>
        <translation>Limites de jogadas e &amp;tempo...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="687"/>
        <source>限制步数和时间(T)</source>
        <translation>Limites de jogadas e tempo (T)</translation>
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
        <translation>Partida em &amp;rede</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="723"/>
        <source>网络对战(I)</source>
        <translation>Partida em rede (I)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="732"/>
        <source>引擎设置(&amp;E)...</source>
        <translation>Configurações do &amp;motor...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="735"/>
        <source>引擎设置(E)</source>
        <translation>Configurações do motor (E)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="750"/>
        <source>电脑执先手(&amp;T)</source>
        <translation>O computador joga &amp;primeiro</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="753"/>
        <source>电脑执先手(T)</source>
        <translation>O computador joga primeiro (T)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="765"/>
        <source>电脑执后手(&amp;R)</source>
        <translation>O computador joga em se&amp;gundo</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="768"/>
        <source>电脑执白(R)</source>
        <translation>O computador joga com as brancas (R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="777"/>
        <source>设置(&amp;O)</source>
        <translation>Configurações (&amp;O)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="788"/>
        <source>工具栏(&amp;T)</source>
        <translation>Barra de &amp;ferramentas</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="799"/>
        <source>信息栏(&amp;D)</source>
        <translation>Painel de informações (&amp;D)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="810"/>
        <source>背景音乐(&amp;M)</source>
        <translation>Música de fundo (&amp;M)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="824"/>
        <source>落子音效(&amp;S)</source>
        <translation>Som das juga&amp;das</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="835"/>
        <source>落子动画(&amp;A)</source>
        <translation>Animação das juga&amp;das</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="838"/>
        <source>落子动画(A)</source>
        <translation>Animação das jogadas (A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="847"/>
        <source>查看帮助(&amp;V)</source>
        <translation>&amp;Ver ajuda</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="850"/>
        <source>F1</source>
        <translation>F1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="859"/>
        <source>作者主页(&amp;W)</source>
        <translation>Página do autor (&amp;W)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="862"/>
        <source>作者主页(W)</source>
        <translation>Página do autor (W)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="867"/>
        <source>关于(&amp;A)...</source>
        <translation>Sobre (&amp;A)...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="870"/>
        <source>关于(A)</source>
        <translation>Sobre (A)</translation>
    </message>
</context>
</TS>
