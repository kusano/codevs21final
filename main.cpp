#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <utility>
#include <algorithm>
#include <fstream>
#include <ctime>
#include "field.h"

using namespace std;



//  ビームサーチ用
struct NODE
{
    long long   score;              //  スコア
    STATE       state;              //  盤面
    int         prev;               //  直前の状態
    MOVE        move;               //  直前の状態からの指手

    NODE() {};
    NODE( long long sc, STATE st, int pr, MOVE mv )
        : score(sc), state(st), prev(pr), move(mv) {}
    bool operator<( const NODE &n ) { return score<n.score; }
};



const int TIME_LIMIT = 3*60*60*CLOCKS_PER_SEC;
//const int TIME_LIMIT = 5*60*CLOCKS_PER_SEC;
ofstream logs;



vector<MOVE> beam( Field &F, int BD, int BW, int origin, int fctarget );
vector<vector<int>> readpack();



int main( int argc, char **argv )
{
    int dummy;
    cin>>dummy>>dummy>>dummy>>dummy>>dummy;

    vector<vector<vector<int>>> pack(N);
    for ( int n=0; n<N; n++ )
        pack[n] = readpack();

    //  ログファイル
    string fname = "log\\log_l_";

    time_t ctm = time(NULL);
    tm ltm;
    localtime_s( &ltm, &ctm );
    char tmp[32];
    strftime( tmp, sizeof tmp, "%Y%m%d_%H%M%S.txt", &ltm );
    fname += tmp;

    //  ログを取らない方は手を変えるため乱数初期化
    if ( argc==2 && strcmp(argv[1],"log")==0 )
        logs = ofstream(fname);
    else
        srand((unsigned int)time(NULL));

    Field F(pack), Fenemy(pack);

    int origin = 0;

    int fctarget = 10;

    vector<MOVE> move;

    int timeleft = TIME_LIMIT;

    for ( int n=0; n<N; n++ )
    {
        bool changepack = false;

        if ( n>0 )
        {
            vector<vector<int>> ps = readpack();
            vector<vector<int>> pe = readpack();

            MOVE m;
            cin >> m.X >> m.R;
            int I;
            Fenemy.move(m, &I);
            logs << "Enemy I: " << I << endl;

            if ( ps!=pack[n] )
            {
                F.setpack(n,ps);
                pack[n] = ps;
                changepack = true;
            }
            Fenemy.setpack(n,pe);

            int obs, obse;
            cin >> obs >> obse;
            logs << "Obstacle: " << obs << " " << obse << endl;

            //  お邪魔ブロックをセット
            //  空きマスよりもお邪魔ブロックが少ない場合でも全てのマスに詰める
            for ( int nn=n+1; nn<N; nn++ )
            {
                vector<vector<int>> p = pack[nn];
                if ( obs>0 )
                {
                    for ( int y=0; y<T; y++ )
                    for ( int x=0; x<T; x++ )
                        if ( p[y][x]==0 || p[y][x]==S+1 )
                            p[y][x] = S+1,
                            obs--;
                }
                else
                {
                    for ( int y=0; y<T; y++ )
                    for ( int x=0; x<T; x++ )
                        if ( p[y][x]==S+1 )
                            p[y][x] = 0;
                }

                if ( p!=pack[nn] )
                {
                    F.setpack(nn,p);
                    pack[nn] = p;
                    changepack = true;
                }
            }
        }

        int start = clock()*1000/CLOCKS_PER_SEC;

        if ( F.getFc() < fctarget )
        {
            int bw = int( (double)timeleft/TIME_LIMIT * 2 * 128 );
            bw = max( bw, 1 );
            bw = min( bw, 128 );

            //  計算済みの手が無い場合、パックの状況が変化した場合に読む
            if ( move.size()==0 || changepack )
                move = beam( F, 32, bw, origin, fctarget );

            //  Fc増加フェーズでは使わないし、
            //  Fc増加フェーズ終了時点が起点になってほしいので、
            //  ここで常に設定しておこう
            origin = n;
        }
        else
        {
            int bw = int( (double)timeleft/TIME_LIMIT * 2 * 64 );
            bw = max( bw, 1 );
            bw = min( bw, 64 );

            //  計算済みの手が無い場合、パックの状況が変化した場合は再探索
            //  残り1手の場合、たぶん連鎖開始なので、これも再探索
            if ( move.size()<=1 || changepack )
                move = beam( F, 24, bw, origin, -1 );
        }

        //  出力
        cout << move[0].X << " " << move[0].R << endl;

        int I;
        F.move(move[0], &I);
        logs << "I: " << I << endl;
        logs << F.tostring() << endl;

        //  連鎖が起きた場合、このステップ数を起点にする
        if ( I>=25 )
            origin = n;

        move.erase(move.begin());

        int used = clock()*1000/CLOCKS_PER_SEC - start;
        timeleft -= used;

        logs << "Time used: " << used << endl;
        logs << "Time left: " << timeleft << endl;

    }

    return 0;
}



//  ビームサーチ
//  BD: 探索深さ
//  BW: ビーム幅
//  origin: 直近の連鎖が発動したステップ
//  fctarget: 非負ならFcがこの値になることを目指す
vector<MOVE> beam( Field &F, int BD, int BW, int origin, int fctarget )
{
    clock_t start = clock();

    logs << "Beam search start" << endl;
    logs << "Step: " << F.getstep() << endl;
    logs << "BD: " << BD << endl;
    logs << "BW: " << BW << endl;
    logs << "origin: " << origin << endl;
    
    Field Finit = F;

    //  直前の状態と、その状態からの指手
    vector<vector<pair<int,MOVE>>> Hist;
    //  ビームサーチのノード
    vector<NODE> Tf;

    Tf.push_back( NODE( 0, F.getstate(), 0, MOVE(0,0) ) );

    for ( int i=0; i<BD && F.getstep()<N; i++ )
    {
        logs << i << " " << Tf[0].score << endl;

        //  Fc増加フェーズの場合、Fcが目的値に達したら終了
        if ( fctarget>=0 && Tf[0].state.Fc>=fctarget )
            break;

        vector<NODE> Pf = Tf;
        Tf.clear();

        for ( int j=0; j<(int)Pf.size(); j++ )
        {
            F.setstate(Pf[j].state);

            for ( int x=-T; x<W; x++ )
            for ( int r=0; r<4; r++ )
            if ( F.check(MOVE(x,r)) )
            {
                F.move( MOVE(x,r) );
                long long s;
                if ( fctarget>=0 )
                    s = F.getFc()*10000000000000000LL+F.getidealscore()-F.getscore();
                else
                    s = F.getidealobs();
                if ( F.getscore()<0 )
                    s = -1;
                Tf.push_back( NODE( s, F.getstate(), j, MOVE(x,r) ) );
                F.undo();
            }
        }

        random_shuffle( Tf.begin(), Tf.end() );
        sort( Tf.begin(), Tf.end() );
        reverse( Tf.begin(), Tf.end() );

        if ( (int)Tf.size() > BW )
            Tf.resize(BW);

        Hist.push_back(vector<pair<int,MOVE>>());
        for ( int j=0; j<(int)Tf.size(); j++ )
            Hist[i].push_back( make_pair(Tf[j].prev,Tf[j].move) );
    }

    vector<MOVE> mv;
    int p=0;
    for ( int i=(int)Hist.size()-1; i>=0; i-- )
        mv.push_back(Hist[i][p].second),
        p = Hist[i][p].first;
    reverse(mv.begin(),mv.end());

    F = Finit;

    if ( fctarget<0 )
    {
        //  最高のスコアが獲得できたステップまで実行
        double maxr = -1e100;
        int maxstep;
        MOVE maxmove;

        for ( int i=0; i<(int)mv.size(); i++ )
        {
            int maxI = -1;
            MOVE mmove;

            for ( int x=-T; x<W; x++ )
            for ( int r=0; r<4; r++ )
            if ( F.check(MOVE(x,r)) )
            {
                int I;
                F.move(MOVE(x,r), &I);
                if ( I > maxI )
                    maxI = I,
                    mmove = MOVE(x,r);
                F.undo();
            }
        
            double rr = (double)maxI/(F.getstep()-origin);
            logs << i << " " << maxI << " " << rr << endl;

            if ( rr>maxr )
            {
                maxr = rr;
                maxstep = i;
                maxmove = mmove;
            }

            F.move(mv[i]);
        }

        logs << "Best: " << maxstep << ", " << maxr << endl;

        mv.resize(maxstep);
        mv.push_back(maxmove);
    }

    F = Finit;

    logs << "Beam search end. time: " << (double)(clock()-start)/CLOCKS_PER_SEC << endl;

    return mv;
}



//  packの読み込み
vector<vector<int>> readpack()
{
    vector<vector<int>> pack(T,vector<int>(T));

    for ( int y=0; y<T; y++ )
    for ( int x=0; x<T; x++ )
        cin>>pack[T-y-1][x];
    string end;
    cin>>end;

    return pack;
}

