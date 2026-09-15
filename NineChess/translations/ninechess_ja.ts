<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ja_JP">
<context>
    <name>GameController</name>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1170"/>
        <source>当前正在浏览历史局面。</source>
        <translation>現在は過去の局面を閲覧しています。</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1171"/>
        <source>是否在此局面下重新开始？悔棋者将承担时间损失！</source>
        <translation>この局面からやり直しますか？待ったをした側は時間を失います！</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1174"/>
        <source>确定</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="../src/gamecontroller.cpp" line="1175"/>
        <source>取消</source>
        <translation>キャンセル</translation>
    </message>
</context>
<context>
    <name>NineChess</name>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="178"/>
        <source>成三棋</source>
        <translation>成三棋</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="179"/>
        <source>打三棋(12连棋)</source>
        <translation>打三棋(12石)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="180"/>
        <source>九连棋</source>
        <translation>九連棋</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="181"/>
        <source>莫里斯九子棋</source>
        <translation>ナインメンズモリス</translation>
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
        <translation>1. 双方とも9個の石を持ち、序盤は交互に石を置きます；
2. 三つ並べると相手の石を1個取れます；
3. 相手の「三連」を構成する石は、他に取れる石がない場合を除いて取れません；
4. 同時に二つの「三連」ができても、取れる石は1個だけです；
5. 置き終わったら交互に動かし、毎回隣接する点へ1歩だけ進めます；
6. 相手の石を3個未満にすると勝ちです；
7. 移動段階で動けなくなった（塞がれた）場合は負けです。</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="192"/>
        <source>1. 双方各12颗子，棋盘有斜线；
2. 摆棋阶段被提子的位置不能再摆子，直到走棋阶段；
3. 摆棋阶段，摆满棋盘算先手负；
4. 走棋阶段，后摆棋的一方先走；
5. 一步出现几个“三连”就可以提几个子；
6. 其它规则与成三棋基本相同。</source>
        <translation>1. 双方とも12個の石を持ち、盤には斜線があります；
2. 配置段階で石を取られた点は、移動段階まで再び使えません；
3. 配置段階で盤が埋まった場合は先手の負けです；
4. 移動段階では、後に置いた側が先に動かします；
5. 一度にいくつ「三連」ができても、その数だけ石を取れます；
6. その他のルールは成三棋とほぼ同じです。</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="199"/>
        <source>1. 规则与成三棋基本相同，只是它的棋子有序号，
2. 相同序号、位置的“三连”不能重复提子；
3. 走棋阶段不能行动（被“闷”），则由对手继续走棋；
4. 一步出现几个“三连”就可以提几个子。</source>
        <translation>1. ルールは成三棋とほぼ同じですが、石に番号が付いています；
2. 同じ番号・同じ位置の「三連」では再度石を取れません；
3. 移動段階で動けない場合、相手がそのまま続けて打ちます；
4. 一度にいくつ「三連」ができても、その数だけ石を取れます。</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="204"/>
        <source>规则与成三棋基本相同，只是在走子阶段，当一方仅剩3子时，他可以飞子到任意空位。</source>
        <translation>ルールは成三棋とほぼ同じですが、移動段階で一方が3個だけになったとき、任意の空点へ石を飛ばすことができます。</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="208"/>
        <source>未开局</source>
        <translation>未開始</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="209"/>
        <source>玩家1</source>
        <translation>プレイヤー1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="210"/>
        <source>玩家2</source>
        <translation>プレイヤー2</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="211"/>
        <source>轮到%1落子，剩余%2子</source>
        <translation>%1の番です。残り%2個を置いてください</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="212"/>
        <source>轮到%1去子，需去%2子</source>
        <translation>%1の番です。%2個取ってください</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="213"/>
        <source>轮到%1选子移动</source>
        <translation>%1の番です。動かす石を選んでください</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="214"/>
        <source>轮到%1落子</source>
        <translation>%1の番です。石を置いてください</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="215"/>
        <source>平局。</source>
        <translation>引き分け。</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="216"/>
        <source>恭喜玩家1获胜！</source>
        <translation>プレイヤー1の勝ちです！</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="217"/>
        <source>恭喜玩家2获胜！</source>
        <translation>プレイヤー2の勝ちです！</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="218"/>
        <source>玩家1认负，恭喜玩家2获胜！</source>
        <translation>プレイヤー1が投了しました。プレイヤー2の勝ちです！</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="219"/>
        <source>玩家2认负，恭喜玩家1获胜！</source>
        <translation>プレイヤー2が投了しました。プレイヤー1の勝ちです！</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="220"/>
        <source>摆满棋盘，恭喜玩家2获胜！</source>
        <translation>盤が埋まりました。プレイヤー2の勝ちです！</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="221"/>
        <source>摆满棋盘，双方平局。</source>
        <translation>盤が埋まりました。引き分けです。</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="222"/>
        <source>玩家1无子可走，恭喜玩家2获胜！</source>
        <translation>プレイヤー1は動かせる石がありません。プレイヤー2の勝ちです！</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="223"/>
        <source>玩家2无子可走，恭喜玩家1获胜！</source>
        <translation>プレイヤー2は動かせる石がありません。プレイヤー1の勝ちです！</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="224"/>
        <source>双方均无子可走，平局。</source>
        <translation>双方とも動かせる石がありません。引き分けです。</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="225"/>
        <source>玩家1超时，恭喜玩家2获胜！</source>
        <translation>プレイヤー1の時間切れです。プレイヤー2の勝ちです！</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="226"/>
        <source>玩家2超时，恭喜玩家1获胜！</source>
        <translation>プレイヤー2の時間切れです。プレイヤー1の勝ちです！</translation>
    </message>
</context>
<context>
    <name>NineChessWindow</name>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="274"/>
        <location filename="../src/ninechesswindow.cpp" line="476"/>
        <location filename="../src/ninechesswindow.cpp" line="1204"/>
        <source>九连棋 v%1</source>
        <translation>九連棋 v%1</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="425"/>
        <location filename="../src/ninechesswindow.cpp" line="495"/>
        <source>语言</source>
        <translation>言語</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 不限时</source>
        <translation> 時間∞</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 不限步</source>
        <translation> 手数∞</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="695"/>
        <source> 限%1步</source>
        <translation> %1手</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="696"/>
        <source> 限时%1分</source>
        <translation> %1分</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="721"/>
        <source>步数和时间限制</source>
        <translation>手数と時間の制限</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="739"/>
        <source>超出限制步数判和：</source>
        <translation>手数の上限を超えたら引き分け：</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="740"/>
        <source>任意一方超时判负：</source>
        <translation>どちらかの時間切れで負け：</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="741"/>
        <location filename="../src/ninechesswindow.cpp" line="745"/>
        <source>无限制</source>
        <translation>無制限</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="742"/>
        <source>50步</source>
        <translation>50手</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="743"/>
        <source>100步</source>
        <translation>100手</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="744"/>
        <source>200步</source>
        <translation>200手</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="746"/>
        <source>5分钟</source>
        <translation>5分</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="747"/>
        <source>10分钟</source>
        <translation>10分</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="748"/>
        <source>20分钟</source>
        <translation>20分</translation>
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
        <translation>キャンセル</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="827"/>
        <source>打开棋谱文件</source>
        <translation>棋譜ファイルを開く</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="839"/>
        <source>文件过大</source>
        <translation>ファイルが大きすぎます</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="839"/>
        <source>不支持1MB以上文件</source>
        <translation>1MBを超えるファイルは対応していません</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="879"/>
        <source>文件错误</source>
        <translation>ファイルエラー</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="879"/>
        <source>不是正确的棋谱文件</source>
        <translation>正しい棋譜ファイルではありません</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="953"/>
        <source>棋谱.txt</source>
        <translation>棋譜.txt</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="955"/>
        <source>保存棋谱文件</source>
        <translation>棋譜ファイルを保存</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1097"/>
        <source>AI设置</source>
        <translation>AI設定</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1121"/>
        <source>玩家1 AI设置</source>
        <translation>プレイヤー1のAI設定</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1122"/>
        <location filename="../src/ninechesswindow.cpp" line="1130"/>
        <source>深度</source>
        <translation>深さ</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1125"/>
        <location filename="../src/ninechesswindow.cpp" line="1133"/>
        <source>限时(秒)</source>
        <translation>制限時間(秒)</translation>
    </message>
    <message>
        <location filename="../src/ninechesswindow.cpp" line="1129"/>
        <source>玩家2 AI设置</source>
        <translation>プレイヤー2のAI設定</translation>
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
        <translation>九連棋</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="71"/>
        <source>文件(&amp;F)</source>
        <translation>ファイル(&amp;F)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="82"/>
        <source>棋局(&amp;C)</source>
        <translation>対局(&amp;C)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="94"/>
        <source>招法(&amp;M)</source>
        <translation>手順(&amp;M)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="107"/>
        <source>引擎(&amp;E)</source>
        <translation>エンジン(&amp;E)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="118"/>
        <source>选项(&amp;O)</source>
        <translation>オプション(&amp;O)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="130"/>
        <source>帮助(&amp;H)</source>
        <translation>ヘルプ(&amp;H)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="138"/>
        <source>规则(&amp;R)</source>
        <translation>ルール(&amp;R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="154"/>
        <source>工具栏</source>
        <translation>ツールバー</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="214"/>
        <source>对战记录</source>
        <translation>対局記録</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="289"/>
        <source>玩家1</source>
        <translation>プレイヤー1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="375"/>
        <source>玩家2</source>
        <translation>プレイヤー2</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="452"/>
        <source>rule</source>
        <translation>rule</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="485"/>
        <source>新建(&amp;N)</source>
        <translation>新規(&amp;N)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="491"/>
        <source>Ctrl+N</source>
        <translation>Ctrl+N</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="500"/>
        <source>打开(&amp;O)...</source>
        <translation>開く(&amp;O)...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="503"/>
        <source>Ctrl+O</source>
        <translation>Ctrl+O</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="512"/>
        <source>保存(&amp;S)</source>
        <translation>保存(&amp;S)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="515"/>
        <source>Ctrl+S</source>
        <translation>Ctrl+S</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="524"/>
        <source>另存为(&amp;A)...</source>
        <translation>名前を付けて保存(&amp;A)...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="529"/>
        <source>退出(&amp;X)</source>
        <translation>終了(&amp;X)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="541"/>
        <source>编辑棋局(&amp;E)</source>
        <translation>局面を編集(&amp;E)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="550"/>
        <source>上下翻转(&amp;F)</source>
        <translation>上下反転(&amp;F)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="559"/>
        <source>左右翻转(&amp;M)</source>
        <translation>左右反転(&amp;M)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="568"/>
        <source>顺时针旋转90°(&amp;R)</source>
        <translation>時計回りに90°回転(&amp;R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="571"/>
        <source>顺时针旋转90°(R)</source>
        <translation>時計回りに90°回転(R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="580"/>
        <source>逆时针旋转90°(&amp;L)</source>
        <translation>反時計回りに90°回転(&amp;L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="583"/>
        <source>逆时针旋转90°(L)</source>
        <translation>反時計回りに90°回転(L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="598"/>
        <source>黑白反转(&amp;B)</source>
        <translation>白黒反転(&amp;B)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="607"/>
        <source>初始局面(&amp;S)</source>
        <translation>初期局面(&amp;S)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="610"/>
        <source>初始局面(S)</source>
        <translation>初期局面(S)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="613"/>
        <source>Ctrl+Up</source>
        <translation>Ctrl+Up</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="622"/>
        <source>前一招(&amp;B)</source>
        <translation>前の手(&amp;B)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="625"/>
        <source>Ctrl+Left</source>
        <translation>Ctrl+Left</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="634"/>
        <source>后一招(&amp;F)</source>
        <translation>次の手(&amp;F)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="637"/>
        <source>后一招(F)</source>
        <translation>次の手(F)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="640"/>
        <source>Ctrl+Right</source>
        <translation>Ctrl+Right</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="649"/>
        <source>最后局面(&amp;E)</source>
        <translation>最終局面(&amp;E)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="652"/>
        <source>Ctrl+Down</source>
        <translation>Ctrl+Down</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="664"/>
        <source>自动演示(&amp;A)</source>
        <translation>自動再生(&amp;A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="667"/>
        <source>自动演示(A)</source>
        <translation>自動再生(A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="676"/>
        <source>认输(&amp;G)</source>
        <translation>投了(&amp;G)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="684"/>
        <source>限制步数和时间(&amp;T)...</source>
        <translation>手数と時間の制限(&amp;T)...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="687"/>
        <source>限制步数和时间(T)</source>
        <translation>手数と時間の制限(T)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="702"/>
        <source>本机对战(&amp;L)</source>
        <translation>ローカル対局(&amp;L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="705"/>
        <source>本机对战(L)</source>
        <translation>ローカル対局(L)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="720"/>
        <source>网络对战(&amp;I)</source>
        <translation>ネットワーク対局(&amp;I)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="723"/>
        <source>网络对战(I)</source>
        <translation>ネットワーク対局(I)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="732"/>
        <source>引擎设置(&amp;E)...</source>
        <translation>エンジン設定(&amp;E)...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="735"/>
        <source>引擎设置(E)</source>
        <translation>エンジン設定(E)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="750"/>
        <source>电脑执先手(&amp;T)</source>
        <translation>コンピュータが先手(&amp;T)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="753"/>
        <source>电脑执先手(T)</source>
        <translation>コンピュータが先手(T)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="765"/>
        <source>电脑执后手(&amp;R)</source>
        <translation>コンピュータが後手(&amp;R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="768"/>
        <source>电脑执白(R)</source>
        <translation>コンピュータが白番(R)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="777"/>
        <source>设置(&amp;O)</source>
        <translation>設定(&amp;O)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="788"/>
        <source>工具栏(&amp;T)</source>
        <translation>ツールバー(&amp;T)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="799"/>
        <source>信息栏(&amp;D)</source>
        <translation>情報パネル(&amp;D)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="810"/>
        <source>背景音乐(&amp;M)</source>
        <translation>BGM(&amp;M)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="824"/>
        <source>落子音效(&amp;S)</source>
        <translation>着手音(&amp;S)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="835"/>
        <source>落子动画(&amp;A)</source>
        <translation>着手アニメーション(&amp;A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="838"/>
        <source>落子动画(A)</source>
        <translation>着手アニメーション(A)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="847"/>
        <source>查看帮助(&amp;V)</source>
        <translation>ヘルプを表示(&amp;V)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="850"/>
        <source>F1</source>
        <translation>F1</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="859"/>
        <source>作者主页(&amp;W)</source>
        <translation>作者のホームページ(&amp;W)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="862"/>
        <source>作者主页(W)</source>
        <translation>作者のホームページ(W)</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="867"/>
        <source>关于(&amp;A)...</source>
        <translation>バージョン情報(&amp;A)...</translation>
    </message>
    <message>
        <location filename="../ninechesswindow.ui" line="870"/>
        <source>关于(A)</source>
        <translation>バージョン情報(A)</translation>
    </message>
</context>
</TS>
