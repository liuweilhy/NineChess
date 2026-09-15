<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru_RU">
<context>
    <name>GameController</name>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1170"/>
        <source>当前正在浏览历史局面。</source>
        <translation>Сейчас просматривается предыдущая позиция.</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1171"/>
        <source>是否在此局面下重新开始？悔棋者将承担时间损失！</source>
        <translation>Начать заново с этой позиции? Взявший ход назад потеряет время!</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1174"/>
        <source>确定</source>
        <translation>ОК</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1175"/>
        <source>取消</source>
        <translation>Отмена</translation>
    </message>
</context>
<context>
    <name>NineChess</name>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="178"/>
        <source>成三棋</source>
        <translation>Чэн Сань Ци</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="179"/>
        <source>打三棋(12连棋)</source>
        <translation>Да Сань Ци (12 фишек)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="180"/>
        <source>九连棋</source>
        <translation>NineChess</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="181"/>
        <source>莫里斯九子棋</source>
        <translation>Мельница</translation>
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
        <translation>1. У каждой стороны по 9 фишек, в начале они выставляются по очереди;
2. Три фишки в ряд позволяют снять одну фишку противника;
3. Нельзя снимать фишку из «тройки» противника, если есть другие фишки для снятия;
4. Если одновременно образовались две «тройки», снимается только одна фишка;
5. После расстановки игроки ходят по очереди на один шаг к соседней точке;
6. Побеждает тот, кто сведёт у противника менее 3 фишек;
7. В фазе ходов невозможность хода (блокировка) означает поражение.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="192"/>
        <source>1. 双方各12颗子，棋盘有斜线；
2. 摆棋阶段被提子的位置不能再摆子，直到走棋阶段；
3. 摆棋阶段，摆满棋盘算先手负；
4. 走棋阶段，后摆棋的一方先走；
5. 一步出现几个“三连”就可以提几个子；
6. 其它规则与成三棋基本相同。</source>
        <translation>1. У каждой стороны по 12 фишек, на доске есть диагонали;
2. Точка, с которой сняли фишку при расстановке, недоступна до фазы ходов;
3. Если при расстановке доска заполнена полностью, первый игрок проигрывает;
4. В фазе ходов первым ходит сторона, выставлявшая фишки второй;
5. Несколько «троек» за один ход позволяют снять столько же фишек;
6. Остальные правила в основном совпадают с Чэн Сань Ци.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="199"/>
        <source>1. 规则与成三棋基本相同，只是它的棋子有序号，
2. 相同序号、位置的“三连”不能重复提子；
3. 走棋阶段不能行动（被“闷”），则由对手继续走棋；
4. 一步出现几个“三连”就可以提几个子。</source>
        <translation>1. Правила в основном совпадают с Чэн Сань Ци, но фишки пронумерованы;
2. «Тройка» с теми же номерами на тех же позициях не позволяет снимать фишки повторно;
3. В фазе ходов при невозможности хода соперник просто продолжает игру;
4. Несколько «троек» за один ход позволяют снять столько же фишек.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="204"/>
        <source>规则与成三棋基本相同，只是在走子阶段，当一方仅剩3子时，他可以飞子到任意空位。</source>
        <translation>Правила в основном совпадают с Чэн Сань Ци, но в фазе ходов сторона, у которой осталось всего 3 фишки, может перелететь фишкой на любую свободную точку.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="208"/>
        <source>未开局</source>
        <translation>Игра не начата</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="209"/>
        <source>玩家1</source>
        <translation>Игрок 1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="210"/>
        <source>玩家2</source>
        <translation>Игрок 2</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="211"/>
        <source>轮到%1落子，剩余%2子</source>
        <translation>Ход %1: осталось выставить %2 шт.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="212"/>
        <source>轮到%1去子，需去%2子</source>
        <translation>Ход %1: снимите %2 шт.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="213"/>
        <source>轮到%1选子移动</source>
        <translation>Ход %1: выберите фишку для хода</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="214"/>
        <source>轮到%1落子</source>
        <translation>Ход %1: выставьте фишку</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="215"/>
        <source>平局。</source>
        <translation>Ничья.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="216"/>
        <source>恭喜玩家1获胜！</source>
        <translation>Поздравляем, игрок 1 победил!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="217"/>
        <source>恭喜玩家2获胜！</source>
        <translation>Поздравляем, игрок 2 победил!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="218"/>
        <source>玩家1认负，恭喜玩家2获胜！</source>
        <translation>Игрок 1 сдался. Поздравляем, игрок 2 победил!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="219"/>
        <source>玩家2认负，恭喜玩家1获胜！</source>
        <translation>Игрок 2 сдался. Поздравляем, игрок 1 победил!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="220"/>
        <source>摆满棋盘，恭喜玩家2获胜！</source>
        <translation>Доска заполнена. Поздравляем, игрок 2 победил!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="221"/>
        <source>摆满棋盘，双方平局。</source>
        <translation>Доска заполнена. Ничья.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="222"/>
        <source>玩家1无子可走，恭喜玩家2获胜！</source>
        <translation>Игрок 1 не может ходить. Поздравляем, игрок 2 победил!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="223"/>
        <source>玩家2无子可走，恭喜玩家1获胜！</source>
        <translation>Игрок 2 не может ходить. Поздравляем, игрок 1 победил!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="224"/>
        <source>双方均无子可走，平局。</source>
        <translation>Ни одна сторона не может ходить. Ничья.</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="225"/>
        <source>玩家1超时，恭喜玩家2获胜！</source>
        <translation>У игрока 1 вышло время. Поздравляем, игрок 2 победил!</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="226"/>
        <source>玩家2超时，恭喜玩家1获胜！</source>
        <translation>У игрока 2 вышло время. Поздравляем, игрок 1 победил!</translation>
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
        <translation>Язык</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 不限时</source>
        <translation> ∞ мин</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 不限步</source>
        <translation> ∞ ходов</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 限%1步</source>
        <translation> %1 ходов</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 限时%1分</source>
        <translation> %1 мин</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="721"/>
        <source>步数和时间限制</source>
        <translation>Ограничение ходов и времени</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="739"/>
        <source>超出限制步数判和：</source>
        <translation>Ничья при превышении лимита ходов:</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="740"/>
        <source>任意一方超时判负：</source>
        <translation>Поражение при просрочке времени любой стороной:</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="741"/>
        <location filename="../src/ninechesswindow.cpp" line="745"/>
        <source>无限制</source>
        <translation>Без ограничений</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="742"/>
        <source>50步</source>
        <translation>50 ходов</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="743"/>
        <source>100步</source>
        <translation>100 ходов</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="744"/>
        <source>200步</source>
        <translation>200 ходов</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="746"/>
        <source>5分钟</source>
        <translation>5 минут</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="747"/>
        <source>10分钟</source>
        <translation>10 минут</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="748"/>
        <source>20分钟</source>
        <translation>20 минут</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="753"/>
        <location filename="../src/ninechesswindow.cpp" line="1139"/>
        <source>确定</source>
        <translation>ОК</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="754"/>
        <location filename="../src/ninechesswindow.cpp" line="1140"/>
        <source>取消</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="827"/>
        <source>打开棋谱文件</source>
        <translation>Открыть запись партии</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="839"/>
        <source>文件过大</source>
        <translation>Файл слишком большой</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="839"/>
        <source>不支持1MB以上文件</source>
        <translation>Файлы больше 1 МБ не поддерживаются</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="879"/>
        <source>文件错误</source>
        <translation>Ошибка файла</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="879"/>
        <source>不是正确的棋谱文件</source>
        <translation>Это не корректный файл записи партии</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="953"/>
        <source>棋谱.txt</source>
        <translation>Партия.txt</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="955"/>
        <source>保存棋谱文件</source>
        <translation>Сохранить запись партии</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1097"/>
        <source>AI设置</source>
        <translation>Настройки ИИ</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1121"/>
        <source>玩家1 AI设置</source>
        <translation>Настройки ИИ игрока 1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1122"/>
        <location filename="../src/ninechesswindow.cpp" line="1130"/>
        <source>深度</source>
        <translation>Глубина</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1125"/>
        <location filename="../src/ninechesswindow.cpp" line="1133"/>
        <source>限时(秒)</source>
        <translation>Время (с)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1129"/>
        <source>玩家2 AI设置</source>
        <translation>Настройки ИИ игрока 2</translation>
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
        <translation>&amp;Файл</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="82"/>
        <source>棋局(&amp;C)</source>
        <translation>&amp;Игра</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="94"/>
        <source>招法(&amp;M)</source>
        <translation>&amp;Ходы</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="107"/>
        <source>引擎(&amp;E)</source>
        <translation>&amp;Движок</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="118"/>
        <source>选项(&amp;O)</source>
        <translation>&amp;Вид</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="130"/>
        <source>帮助(&amp;H)</source>
        <translation>&amp;Справка</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="138"/>
        <source>规则(&amp;R)</source>
        <translation>&amp;Правила</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="154"/>
        <source>工具栏</source>
        <translation>Панель инструментов</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="214"/>
        <source>对战记录</source>
        <translation>Запись партии</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="289"/>
        <source>玩家1</source>
        <translation>Игрок 1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="375"/>
        <source>玩家2</source>
        <translation>Игрок 2</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="452"/>
        <source>rule</source>
        <translation>rule</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="485"/>
        <source>新建(&amp;N)</source>
        <translation>&amp;Новый</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="491"/>
        <source>Ctrl+N</source>
        <translation>Ctrl+N</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="500"/>
        <source>打开(&amp;O)...</source>
        <translation>&amp;Открыть...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="503"/>
        <source>Ctrl+O</source>
        <translation>Ctrl+O</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="512"/>
        <source>保存(&amp;S)</source>
        <translation>&amp;Сохранить</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="515"/>
        <source>Ctrl+S</source>
        <translation>Ctrl+S</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="524"/>
        <source>另存为(&amp;A)...</source>
        <translation>Сохранить &amp;как...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="529"/>
        <source>退出(&amp;X)</source>
        <translation>В&amp;ыход</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="541"/>
        <source>编辑棋局(&amp;E)</source>
        <translation>&amp;Редактировать позицию</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="550"/>
        <source>上下翻转(&amp;F)</source>
        <translation>Отразить по &amp;вертикали</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="559"/>
        <source>左右翻转(&amp;M)</source>
        <translation>Отразить по &amp;горизонтали</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="568"/>
        <source>顺时针旋转90°(&amp;R)</source>
        <translation>Повернуть на 90° по часовой (&amp;R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="571"/>
        <source>顺时针旋转90°(R)</source>
        <translation>Повернуть на 90° по часовой (R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="580"/>
        <source>逆时针旋转90°(&amp;L)</source>
        <translation>Повернуть на 90° против часовой (&amp;L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="583"/>
        <source>逆时针旋转90°(L)</source>
        <translation>Повернуть на 90° против часовой (L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="598"/>
        <source>黑白反转(&amp;B)</source>
        <translation>Сменить &amp;цвета</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="607"/>
        <source>初始局面(&amp;S)</source>
        <translation>&amp;Начальная позиция</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="610"/>
        <source>初始局面(S)</source>
        <translation>Начальная позиция (Н)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="613"/>
        <source>Ctrl+Up</source>
        <translation>Ctrl+Up</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="622"/>
        <source>前一招(&amp;B)</source>
        <translation>&amp;Предыдущий ход</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="625"/>
        <source>Ctrl+Left</source>
        <translation>Ctrl+Left</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="634"/>
        <source>后一招(&amp;F)</source>
        <translation>Следующий &amp;ход</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="637"/>
        <source>后一招(F)</source>
        <translation>Следующий ход (Х)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="640"/>
        <source>Ctrl+Right</source>
        <translation>Ctrl+Right</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="649"/>
        <source>最后局面(&amp;E)</source>
        <translation>&amp;Конечная позиция</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="652"/>
        <source>Ctrl+Down</source>
        <translation>Ctrl+Down</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="664"/>
        <source>自动演示(&amp;A)</source>
        <translation>&amp;Автовоспроизведение</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="667"/>
        <source>自动演示(A)</source>
        <translation>Автовоспроизведение (А)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="676"/>
        <source>认输(&amp;G)</source>
        <translation>&amp;Сдаться</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="684"/>
        <source>限制步数和时间(&amp;T)...</source>
        <translation>Ограничение ходов и &amp;времени...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="687"/>
        <source>限制步数和时间(T)</source>
        <translation>Ограничение ходов и времени (В)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="702"/>
        <source>本机对战(&amp;L)</source>
        <translation>&amp;Локальная игра</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="705"/>
        <source>本机对战(L)</source>
        <translation>Локальная игра (Л)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="720"/>
        <source>网络对战(&amp;I)</source>
        <translation>Игра по &amp;сети</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="723"/>
        <source>网络对战(I)</source>
        <translation>Игра по сети (С)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="732"/>
        <source>引擎设置(&amp;E)...</source>
        <translation>&amp;Настройки движка...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="735"/>
        <source>引擎设置(E)</source>
        <translation>Настройки движка (Н)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="750"/>
        <source>电脑执先手(&amp;T)</source>
        <translation>Компьютер &amp;ходит первым</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="753"/>
        <source>电脑执先手(T)</source>
        <translation>Компьютер ходит первым (Х)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="765"/>
        <source>电脑执后手(&amp;R)</source>
        <translation>Компьютер ходит &amp;вторым</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="768"/>
        <source>电脑执白(R)</source>
        <translation>Компьютер играет белыми (В)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="777"/>
        <source>设置(&amp;O)</source>
        <translation>&amp;Настройки</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="788"/>
        <source>工具栏(&amp;T)</source>
        <translation>&amp;Панель инструментов</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="799"/>
        <source>信息栏(&amp;D)</source>
        <translation>&amp;Информационная панель</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="810"/>
        <source>背景音乐(&amp;M)</source>
        <translation>Фоновая &amp;музыка</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="824"/>
        <source>落子音效(&amp;S)</source>
        <translation>&amp;Звук ходов</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="835"/>
        <source>落子动画(&amp;A)</source>
        <translation>&amp;Анимация ходов</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="838"/>
        <source>落子动画(A)</source>
        <translation>Анимация ходов (А)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="847"/>
        <source>查看帮助(&amp;V)</source>
        <translation>&amp;Просмотр справки</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="850"/>
        <source>F1</source>
        <translation>F1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="859"/>
        <source>作者主页(&amp;W)</source>
        <translation>Домашняя страница &amp;автора</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="862"/>
        <source>作者主页(W)</source>
        <translation>Домашняя страница автора (А)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="867"/>
        <source>关于(&amp;A)...</source>
        <translation>&amp;О программе...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="870"/>
        <source>关于(A)</source>
        <translation>О программе (О)</translation>
    </message>
</context>
</TS>
