#pragma once
#include "Renderer.h"
#include "World.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <sstream>

// Authored starting village surrounded by deterministic streamed chunks.
class Prototype {
    using Object=WorldObject;
    World world;
    std::vector<Object> villageObjects;
    Renderer& r;
    int width=1280, height=800;
    bool keys[256] = {};
    bool arrows[4] = {};
    std::vector<Object> objects;
    WorldPoint player={70,160}, spirit={300,160}, camera={70,160};
    const WorldPoint child={-100,90}, keeper={150,-100}, fire={0,0};
    float time=0, walk=0, messageTime=0;
    bool moving=false, captured=false, controlling=false, journal=false, toySword=false;
    int childTalk=0;
    std::string speaker, message;
    static double Distance(WorldPoint a, WorldPoint b) {
        double x=a.x-b.x,y=a.y-b.y; return std::sqrt(x*x+y*y);
    }
    Point Screen(double x,double y,double z=0) const {
        x-=camera.x; y-=camera.y;
        return {static_cast<float>(width*.5+(x-y)*.85),static_cast<float>(height*.51+(x+y)*.43-z)};
    }
    void Ground(double x,double y,double w,double d,Color c) {
        r.Quad(Screen(x,y),Screen(x+w,y),Screen(x+w,y+d),Screen(x,y+d),c);
    }
    void Box(double x,double y,double w,double d,double h,Color top,Color left,Color right) {
        Point a=Screen(x,y,h),b=Screen(x+w,y,h),c=Screen(x+w,y+d,h),e=Screen(x,y+d,h);
        r.Quad(e,c,Screen(x+w,y+d),Screen(x,y+d),left);
        r.Quad(b,Screen(x+w,y),Screen(x+w,y+d),c,right);
        r.Quad(a,b,c,e,top);
    }
    void Say(const std::string& name,const std::string& text) {
        speaker=name; message=text; messageTime=8;
    }
    bool Blocked(WorldPoint p) {
        if(world.Blocked(p)) return true;
        for (const Object& o:villageObjects) {
            if (p.x>o.x-12 && p.x<o.x+o.w+12 && p.y>o.y-12 && p.y<o.y+o.d+12) return true;
        }
        return false;
    }
    int Nearby() const {
        if (controlling) return -1;
        double best=90; int target=-1;
        const WorldPoint positions[]={child,keeper,spirit,fire};
        for(int i=0;i<4;++i) {
            if(i==2 && captured) continue;
            double d=Distance(player,positions[i]);
            if(d<best) { best=d; target=i; }
        }
        return target;
    }
    void Interact() {
        switch(Nearby()) {
        case 0:
            if (!toySword) {
                toySword=true;
                Say("마을 아이 · 미라", "이 나무칼 가져가. 어둠이 찾아오면 꼭 쥐고 있어.");
            } else {
                ++childTalk;
                Say("마을 아이 · 미라", childTalk%2 ?
                    "아직 가지고 있네. 엄마가 그랬어. 작은 물건도 다정한 마음을 기억한대." :
                    "무덤 곁의 푸른 빛 말이야… 불 가까이에는 오지 않아.");
            }
            break;
        case 1: Say("불씨지기", "불씨 하나면 충분하오. 잠시 쉬어 가시오, 나그네. 길은 달아나지 않으니."); break;
        case 2:
            captured=true;
            Say("도감 · 새로운 기록", "슬픔의 도깨비불을 포획했습니다. Q로 조종하고 J로 기록을 살펴보세요.");
            break;
        case 3: Say("마지막 불씨", "잠시나마 마을이 안전하게 느껴진다. 아직 누군가 이 불을 지키고 있다."); break;
        default: break;
        }
    }
    void House(const Object& o) {
        Box(o.x,o.y,o.w,o.d,o.h,Color(.23f,.25f,.25f),Color(.19f,.21f,.22f),Color(.12f,.15f,.17f));
        Point a=Screen(o.x-12,o.y-12,o.h),b=Screen(o.x+o.w+12,o.y-12,o.h);
        Point c=Screen(o.x+o.w+12,o.y+o.d+12,o.h),d=Screen(o.x-12,o.y+o.d+12,o.h);
        Point ridgeA=Screen(o.x-12,o.y+o.d*.5f,o.h+65),ridgeB=Screen(o.x+o.w+12,o.y+o.d*.5f,o.h+65);
        r.Triangle(a,d,ridgeA,Color(.16f,.19f,.20f));
        r.Quad(ridgeA,ridgeB,c,d,Color(.27f,.19f,.18f));
        r.Triangle(b,c,ridgeB,Color(.19f,.13f,.14f));
        r.Line(ridgeA,ridgeB,3,Color(.38f,.29f,.24f));
        for(int i=1;i<5;++i) {
            float u=float(i)/5;
            r.Line({ridgeA.x+(ridgeB.x-ridgeA.x)*u,ridgeA.y+(ridgeB.y-ridgeA.y)*u},
                {d.x+(c.x-d.x)*u,d.y+(c.y-d.y)*u},2,Color(.16f,.13f,.14f));
        }
        // Front-wall door and windows lie on the same projected plane as the wall.
        double front=o.y+o.d;
        r.Quad(Screen(o.x+o.w*.42f,front,0),Screen(o.x+o.w*.65f,front,0),
            Screen(o.x+o.w*.65f,front,48),Screen(o.x+o.w*.42f,front,48),Color(.07f,.09f,.10f));
        r.Quad(Screen(o.x+18,front,38),Screen(o.x+40,front,38),
            Screen(o.x+40,front,62),Screen(o.x+18,front,62),Color(2.2f,1.25f,.45f));
        Box(o.x+o.w*.7f,o.y+18,18,20,o.h+70,Color(.32f,.32f,.29f),Color(.23f,.23f,.23f),Color(.16f,.17f,.18f));
    }
    void Tree(const Object& o) {
        Point p=Screen(o.x+o.w*.5f,o.y+o.d*.5f);
        r.Ellipse(p.x,p.y,27,11,Color(0,0,0,.25f));
        r.Line(p,{p.x-4,static_cast<float>(p.y-o.h)},7,Color(.17f,.16f,.17f));
        for(int i=0;i<4;++i) {
            float side=i%2 ? 1.f : -1.f, level=static_cast<float>(p.y-o.h*.35-i*15);
            Point tip={p.x+side*(23+i*3),level-28};
            r.Line({p.x-2,level},tip,4,Color(.19f,.18f,.19f));
            r.Line(tip,{tip.x+side*10,tip.y-21},2,Color(.22f,.21f,.22f));
        }
    }
    void Person(WorldPoint pos, Color coat, bool active, bool childSize=false) {
        Point p=Screen(pos.x,pos.y);
        float scale=childSize?.8f:1.f;
        float sway=active&&moving?std::sin(walk)*2:0;
        r.Ellipse(p.x,p.y+1,15*scale,6,Color(0,0,0,.4f));
        if(active) r.Ellipse(p.x,p.y,19,8,Color(.75f,.69f,.45f,.22f));
        r.Line({p.x-5,p.y-12*scale},{p.x-6+sway,p.y},4,Color(.11f,.12f,.14f));
        r.Line({p.x+5,p.y-12*scale},{p.x+6-sway,p.y},4,Color(.11f,.12f,.14f));
        r.Triangle({p.x,p.y-43*scale},{p.x-15*scale,p.y-8*scale},{p.x+15*scale,p.y-8*scale},coat);
        r.Triangle({p.x,p.y-40*scale},{p.x+2,p.y-8*scale},{p.x+15*scale,p.y-8*scale},
            Color(coat.r*.65f,coat.g*.65f,coat.b*.65f));
        r.Ellipse(p.x,p.y-44*scale,9*scale,11*scale,Color(.17f,.18f,.20f));
        r.Rect(p.x-4*scale,p.y-46*scale,9*scale,8*scale,Color(.61f,.53f,.43f));
        if(active) {
            r.Line({p.x+12,p.y-30},{p.x+18,p.y-3},3,Color(.58f,.54f,.42f));
            r.Rect(p.x-11,p.y-34,21,4,Color(.48f,.31f,.26f));
        }
    }
    void Wisp() {
        Point p=Screen(spirit.x,spirit.y);
        float bob=std::sin(time*2.6f)*5;
        r.Ellipse(p.x,p.y,19,7,Color(.12f,.61f,.66f,.17f));
        r.Triangle({p.x-12,p.y-23+bob},{p.x+12,p.y-23+bob},{p.x-4,p.y+1+bob},Color(.25f,.66f,.70f,.6f));
        r.Ellipse(p.x,p.y-29+bob,14,17,Color(.31f,.74f,.77f,.8f));
        r.Ellipse(p.x-2,p.y-31+bob,8,11,Color(.85f,1.8f,1.55f,.85f));
        r.Rect(p.x-6,p.y-33+bob,3,4,Color(.06f,.17f,.22f));
        r.Rect(p.x+3,p.y-33+bob,3,4,Color(.06f,.17f,.22f));
        for(int i=0;i<5;++i) {
            float a=time+i*1.25f;
            r.Ellipse(p.x+std::cos(a)*24,p.y-25+std::sin(a*1.4f)*18,1.5f,1.5f,Color(.50f,.91f,.83f,.7f));
        }
    }
    void Hearth() {
        Point p=Screen(0,0);
        r.Ellipse(p.x,p.y,28,13,Color(.29f,.28f,.26f));
        r.Ellipse(p.x,p.y,22,9,Color(.09f,.10f,.11f));
        r.Line({p.x-15,p.y+3},{p.x+12,p.y-5},6,Color(.36f,.23f,.16f));
        float flicker=std::sin(time*13)*3;
        r.Triangle({p.x-13,p.y},{p.x+11,p.y},{p.x+3,p.y-40-flicker},Color(2.4f,.9f,.25f,.9f));
        r.Triangle({p.x-7,p.y},{p.x+8,p.y},{p.x-2,p.y-25+flicker},Color(3.f,1.8f,.6f));
        for(int i=0;i<7;++i) {
            float t=std::fmod(time*18+i*12.f,85.f);
            r.Ellipse(p.x+std::sin(t*.08f+i)*12,p.y-t,1.3f,2,Color(1,.66f,.30f,1-t/85));
        }
    }
    void Panel(float x,float y,float w,float h) {
        r.Rect(x,y,w,h,Color(.035f,.052f,.068f,.94f));
        r.Rect(x,y,w,1,Color(.48f,.43f,.30f,.65f));
        r.Rect(x,y+h-1,w,1,Color(.28f,.30f,.29f,.6f));
    }
    void HUD() {
        Color ivory(.86f,.84f,.74f),muted(.49f,.57f,.58f),gold(.82f,.66f,.39f);
        Panel(24,24,268,100);
        r.Text(42,50,"마지막 불씨",ivory,true);
        r.Text(42,72,world.Region(controlling?spirit:player),muted);
        r.Rect(42,87,184,7,Color(.20f,.17f,.18f));
        r.Rect(42,87,184,7,Color(.57f,.29f,.25f));
        r.Text(237,96,"100",ivory);
        r.Text(42,114,controlling?"조종 중: 슬픔의 도깨비불":"조종 중: 방랑자",gold);
        Panel(float(width-285),24,261,106);
        r.Text(float(width-267),49,"탐험 기록",gold);
        r.Text(float(width-267),71,captured?"주령 기록       01 / 01":"주령 미발견     00 / 01",ivory);
        r.Text(float(width-267),93,captured?"체험 포획: 사용 완료":"체험 포획: 1회 가능",muted);
        r.Text(float(width-267),115,toySword?"간직한 물건: 나무칼":"불가의 아이를 찾아보세요.",muted);
        Panel(24,float(height-55),float(width-48),32);
        r.Text(40,float(height-34),"WASD / 방향키  이동    Shift  달리기    E  상호작용    Q  주령 조종    J  도감    P  화면 효과    B  블룸    ESC  닫기",ivory);
        WorldPoint location=controlling?spirit:player;
        std::ostringstream positionText;
        positionText<<"지역 좌표 "<<World::Index(location.x)<<", "<<World::Index(location.y)<<" · 주변 구역 "<<world.chunks.size();
        r.Text(42,146,positionText.str(),muted);
        const double homeX=(-location.x+location.y)*.85,homeY=(-location.x-location.y)*.43;
        const char* homeDirection=std::abs(homeX)>std::abs(homeY)?(homeX>0?"오른쪽":"왼쪽"):(homeY>0?"아래쪽":"위쪽");
        std::ostringstream homeText; homeText<<"마을 불씨: "<<homeDirection<<" · 거리 "<<static_cast<long long>(Distance(location,fire));
        r.Text(42,168,homeText.str(),muted);
        int target=Nearby();
        if(target>=0 && !journal) {
            const char* hints[]={"[E] 미라와 대화하기","[E] 불씨지기와 대화하기","[E] 슬픔의 도깨비불 포획하기","[E] 불가에서 쉬기"};
            Panel(width*.5f-160,float(height-104),320,32);
            r.Text(width*.5f-143,float(height-83),hints[target],gold);
        }
        if(messageTime>0 && !journal) {
            Panel(40,float(height-206),float(width-80),88);
            r.Text(60,float(height-179),speaker,gold);
            r.Text(60,float(height-151),message,ivory);
        }
        if(journal) {
            r.Rect(0,0,float(width),float(height),Color(0,0,0,.58f));
            float x=width*.5f-315,y=height*.5f-190;
            Panel(x,y,630,380);
            r.Text(x+28,y+38,"도감 · 보이지 않는 존재들의 기록",gold,true);
            r.Text(x+28,y+76,captured?"01  슬픔의 도깨비불 · 포획 완료":"01  미지의 존재 · 푸른 빛을 따라가세요",ivory);
            r.Text(x+28,y+108,captured?"돌아오지 못한 이들의 슬픔에서 태어난 주령.":"불가의 동쪽 어딘가에서 작은 빛이 기다리고 있습니다.",muted);
            r.Text(x+28,y+135,captured?"발견 장소: 잿빛 마을, 오래된 무덤 곁.":"가까이 다가가 E를 누르면 포획할 수 있습니다.",muted);
            r.Text(x+28,y+162,captured?"Q로 조종을 전환합니다. 본체는 그 자리에 남습니다.":"아직 이 존재의 이야기가 기록되지 않았습니다.",muted);
            r.Text(x+28,y+211,"간직한 물건",gold);
            r.Text(x+28,y+239,toySword?"나무칼 · 미라가 건넨 선물":"아직 간직한 물건이 없습니다.",ivory);
            r.Text(x+28,y+267,toySword?"작은 다정함의 흔적. 훗날 어떤 의미가 될지는 아직 모릅니다.":"아직 이곳을 집이라 부르는 사람들과 이야기해 보세요.",muted);
            r.Text(x+28,y+329,"체험 기록은 종료 시 사라집니다. J 또는 ESC로 돌아갑니다.",gold);
        }
    }
public:
    explicit Prototype(Renderer& renderer):r(renderer) {
        villageObjects={{0,-300,-210,140,110,92},{0,-70,-360,155,115,115},
            {0,-440,90,130,100,85},{0,270,-250,145,110,98},
            {2,330,240,65,35,30},{2,390,270,30,35,52},
            {2,280,280,25,25,24}};
        world.Stream(camera,player,width,height);
    }
    void Resize(int w,int h) {width=w;height=h;r.Resize(w,h);}
    void Key(unsigned char key,bool down) {
        if(key>='A' && key<='Z') key=static_cast<unsigned char>(key-'A'+'a');
        bool fresh=down&&!keys[key]; keys[key]=down;
        if(!fresh) return;
        if(key==27) { if(journal) journal=false; else messageTime=0; }
        if(key=='p') { r.postProcess.enabled=!r.postProcess.enabled; Say("화면 효과",r.postProcess.enabled?"후처리를 켰습니다.":"후처리를 껐습니다."); }
        if(key=='b') { r.postProcess.bloomEnabled=!r.postProcess.bloomEnabled; Say("빛 번짐",r.postProcess.bloomEnabled?"블룸을 켰습니다. 후처리도 켜져 있어야 적용됩니다.":"블룸을 껐습니다."); }
        if(key=='j') journal=!journal;
        if(journal) return;
        if(key=='e') Interact();
        if(key=='q') {
            if(captured) { controlling=!controlling; Say("조종 전환",controlling?"주령을 조종합니다. WASD로 이동하고 Q로 본체에 돌아갑니다.":"방랑자의 몸으로 돌아왔습니다."); }
            else Say("조종 전환","먼저 주령을 포획하세요. 불가 동쪽의 푸른 빛을 따라가 보세요.");
        }
    }
    void Arrow(int key,bool down) {if(key>=0&&key<4) arrows[key]=down;}
    void ClearInput() { for(bool& k:keys) k=false; for(bool& k:arrows) k=false; }
    void Update(float dt, bool sprint = false) {
        time+=dt; messageTime=(std::max)(0.f,messageTime-dt);
        moving=false;
        if(!journal) {
            float sx=float(keys['d']||arrows[2])-float(keys['a']||arrows[0]);
            float sy=float(keys['s']||arrows[3])-float(keys['w']||arrows[1]);
            // Invert the isometric projection so keys correspond to screen directions.
            float dx=sx/.85f+sy/.43f,dy=-sx/.85f+sy/.43f;
            float length=std::sqrt(dx*dx+dy*dy);
            if(length>0) {
                const float pace = sprint ? 1.8f : 1.f;
                float speed=(controlling?160.f:125.f)*pace;
                dx=dx/length*speed*dt;dy=dy/length*speed*dt;
                WorldPoint& actor=controlling?spirit:player;
                WorldPoint next={actor.x+dx,actor.y}; if(!Blocked(next)) actor=next;
                next={actor.x,actor.y+dy}; if(!Blocked(next)) actor=next;
                moving=true; walk+=dt*10*pace;
            }
            if(captured&&!controlling) {
                WorldPoint target={player.x+35,player.y+20};
                float t=1-std::exp(-dt*3); spirit.x+=(target.x-spirit.x)*t; spirit.y+=(target.y-spirit.y)*t;
            }
        }
        WorldPoint target=controlling?spirit:player;
        if(Distance(target,camera)>2000) camera=target; // Distant vessel switch.
        float t=1-std::exp(-dt*6);camera.x+=(target.x-camera.x)*t;camera.y+=(target.y-camera.y)*t;
        world.Stream(camera,target,width,height);
    }
    void Draw() {
        r.Begin(Color(.035f,.052f,.069f));
        objects=villageObjects;
        for(const auto& pair:world.chunks) {
            const World::Chunk& chunk=pair.second;
            for(const Object& o:chunk.objects) objects.push_back(o);
            for(int y=0;y<8;++y) for(int x=0;x<8;++x) {
                World::Coordinate tx=chunk.key.first*8+x,ty=chunk.key.second*8+y;
                double wx=double(tx)*64,wy=double(ty)*64;
                Point p=Screen(wx,wy);
                if(p.x < -120 || p.x > width+120 || p.y < -100 || p.y > height+100) continue;
                auto hash=World::Hash(tx,ty,42);
                float shade=float(hash%15)*.002f;
                Color color=chunk.biome==0?Color(.095f+shade,.125f+shade,.13f+shade):
                    (chunk.biome==1?Color(.12f+shade,.145f+shade,.145f+shade):Color(.15f+shade,.14f+shade,.14f+shade));
                if(World::Village(wx,wy)) color=Color(.11f+shade,.13f+shade,.13f+shade);
                bool road=World::Road(tx,ty);
                if(road) color=Color(.23f+shade,.23f+shade,.20f+shade);
                Ground(wx,wy,64,64,color);
                if(road) Ground(wx+12+double(hash%17),wy+15,16,10,Color(.32f,.31f,.26f,.5f));
                else r.Line(p,{p.x+3,p.y-4},1,Color(.24f,.27f,.24f,.4f));
            }
        }
        Ground(-165,-145,330,290,Color(.24f,.24f,.21f));
        struct Entry {double depth;int index;};
        std::vector<Entry> order;
        for(size_t i=0;i<objects.size();++i) {
            const Object& o=objects[i];
            Point bounds=Screen(o.x+o.w*.5,o.y+o.d*.5);
            if(bounds.x < -300 || bounds.x > width+300 || bounds.y < -250 || bounds.y > height+350) continue;
            order.push_back({o.x+o.y+o.w+o.d,static_cast<int>(i)});
        }
        order.push_back({0,-1});order.push_back({child.x+child.y,-2});
        order.push_back({keeper.x+keeper.y,-3});order.push_back({player.x+player.y,-4});
        order.push_back({spirit.x+spirit.y,-5});
        std::stable_sort(order.begin(),order.end(),[](const Entry& a,const Entry& b){return a.depth<b.depth;});
        for(const Entry& entry:order) {
            if(entry.index>=0) {
                const Object& o=objects[entry.index];
                if(o.kind==0) House(o);
                else if(o.kind==1) Tree(o);
                else Box(o.x,o.y,o.w,o.d,o.h,Color(.33f,.36f,.35f),Color(.23f,.27f,.28f),Color(.17f,.21f,.23f));
            } else if(entry.index==-1) Hearth();
            else if(entry.index==-2) Person(child,Color(.48f,.36f,.29f),false,true);
            else if(entry.index==-3) Person(keeper,Color(.33f,.37f,.34f),false);
            else if(entry.index==-4) Person(player,Color(.44f,.49f,.52f),!controlling);
            else Wisp();
        }
        // Moving low fog and airborne ash, deliberately subtle over the scene.
        for(int i=0;i<9;++i) {
            float x=std::fmod(i*197.f+time*9,width+350.f)-175;
            r.Ellipse(x,height*.25f+i*67,210,20,Color(.38f,.48f,.51f,.025f));
        }
        for(int i=0;i<40;++i) {
            float x=std::fmod(i*131.f+time*13,width+10.f);
            float y=std::fmod(i*73.f+time*8,height+10.f);
            r.Rect(x,y,2,2,Color(.67f,.65f,.55f,.2f));
        }
        r.FinishScene(); // Post-process the world before drawing crisp UI and text.
        // Framing bars keep the world visually quiet behind the interface.
        r.Rect(0,0,float(width),10,Color(.02f,.03f,.04f));
        HUD();r.Flush();
    }
};





