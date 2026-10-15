'use strict';
const $ = id => document.getElementById(id);
const records = new Map();
let connected = false, offset = 0, generation = 0, controller, token = '', lastStreamAt = 0;
let selectedNpc = '', mapTargets = [];
const roleColors = {farmer:'#80c89a',merchant:'#7eaff8',artisan:'#e4ad72',hunter:'#c8a2f7',story:'#b9c7d4'};
const names = {title:'타이틀',opening:'오프닝 감상 중',playing:'플레이 중',paused:'일시 정지',dialogue:'대화·선택 중',dead:'플레이어 사망',
  present:'배치됨',talking:'대화 중',waiting:'대기',inactive:'현재 미등장',missing:'실종',combat:'전투 중',
  monsters_present:'몬스터 관측',no_observed_threat:'관측된 위협 없음',weakened:'약화',remnant:'포획 잔영',returning:'복귀 중',idle:'대기',offline:'수신 중단',
  walking:'이동 중',working:'일하는 중',trading:'거래 중',hunting:'주령 수색',fighting:'사냥 중',resting:'휴식 중',recovering:'회복 중',fleeing:'대피 중'};
const keyOf = s => JSON.stringify([s.game_id,s.source_id]);
function element(tag,text,className){const e=document.createElement(tag);if(text!==undefined)e.textContent=text;if(className)e.className=className;return e;}
function badge(status,label){return element('span',label || names[status] || status,'badge '+(Object.hasOwn(names,status)?status:'waiting'));}
function ageOf(record){return Math.max(0,Date.now()+offset-record.received_at_ms);}
function live(record){return connected && ageOf(record)<4000;}
function sourceLabel(s){return s.game_name+' / '+s.source_id;}
function empty(parent,text,columns){const e=element(columns?'td':'div',text,'empty');if(columns){e.colSpan=columns;const row=element('tr');row.append(e);parent.append(row);}else parent.append(e);}
function cell(row,text){const td=element('td',text);row.append(td);return td;}
function selected(){const key=$('source').value;return [...records.values()].filter(r=>!key||keyOf(r.snapshot)===key||key==='game:'+r.snapshot.game_id);}
function updateOptions(){
  const select=$('source'), old=select.value;
  select.replaceChildren(new Option('모든 게임 · 통합 보기',''));
  const games=new Map([...records.values()].map(r=>[r.snapshot.game_id,r.snapshot.game_name]));
  for(const [id,name] of games)select.add(new Option(name+' · 모든 실행 인스턴스','game:'+id));
  for(const r of records.values())select.add(new Option(sourceLabel(r.snapshot),keyOf(r.snapshot)));
  if([...select.options].some(o=>o.value===old))select.value=old;
}
const npcKey = (snapshot,npc) => JSON.stringify([snapshot.game_id,snapshot.source_id,npc.id]);
const coordinates = p => p ? `${Math.round(p.x)}, ${Math.round(p.y)}` : '—';
function npcVisible(npc){
  const search=$('search').value.trim().toLowerCase(), role=$('role-filter').value;
  return `${npc.name} ${npc.role_name||''}`.toLowerCase().includes(search) &&
    (!role || (role==='story'?npc.kind!=='autonomous':npc.role===role)) &&
    (!$('active-only').checked||npc.active);
}
function renderMap(all){
  const canvas=$('world-map'),ctx=canvas.getContext('2d');
  if(!ctx)return;
  const w=canvas.width,h=canvas.height, points=[];mapTargets=[];
  for(const record of all){
    const s=record.snapshot;
    for(const npc of s.npcs)if(npc.active&&npc.position&&npcVisible(npc))points.push({p:npc.position,npc,s,record});
    for(const mob of s.monsters)if(mob.position)points.push({p:mob.position,mob,s,record});
    if(!['title','opening'].includes(s.mode)&&s.player?.position)points.push({p:s.player.position,player:true,s,record});
  }
  ctx.fillStyle='#0e1b25';ctx.fillRect(0,0,w,h);
  if(!points.length){ctx.fillStyle='#8da6b5';ctx.font='18px sans-serif';ctx.textAlign='center';ctx.fillText('게임에서 위치 메시지를 기다리고 있습니다.',w/2,h/2);$('npc-detail').textContent='현재 표시할 위치 정보가 없습니다.';return;}
  let minX=Infinity,maxX=-Infinity,minY=Infinity,maxY=-Infinity;
  for(const {p} of points){minX=Math.min(minX,p.x);maxX=Math.max(maxX,p.x);minY=Math.min(minY,p.y);maxY=Math.max(maxY,p.y);}
  minX-=160;maxX+=160;minY-=160;maxY+=160;
  const scale=Math.min((w-80)/Math.max(800,maxX-minX),(h-70)/Math.max(600,maxY-minY));
  const project=p=>({x:w/2+(p.x-(minX+maxX)/2)*scale,y:h/2+(p.y-(minY+maxY)/2)*scale});
  ctx.strokeStyle='#203340';ctx.lineWidth=1;
  for(let x=0;x<w;x+=64){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);ctx.stroke();}
  for(let y=0;y<h;y+=64){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke();}
  for(const object of points){
    const at=project(object.p),active=live(object.record);ctx.globalAlpha=active?1:.3;
    if(object.npc){
      const npc=object.npc,key=npcKey(object.s,npc),picked=key===selectedNpc;
      const color=roleColors[npc.role]||roleColors.story;
      if(picked&&npc.destination){const end=project(npc.destination);ctx.strokeStyle=color;ctx.setLineDash([5,5]);ctx.beginPath();ctx.moveTo(at.x,at.y);ctx.lineTo(end.x,end.y);ctx.stroke();ctx.setLineDash([]);ctx.strokeRect(end.x-4,end.y-4,8,8);}
      ctx.fillStyle=color;ctx.beginPath();ctx.arc(at.x,at.y,picked?7:4,0,Math.PI*2);ctx.fill();
      if(picked||npc.status==='fighting'){ctx.strokeStyle=picked?'#fff':'#f08d9d';ctx.beginPath();ctx.arc(at.x,at.y,9,0,Math.PI*2);ctx.stroke();}
      mapTargets.push({x:at.x,y:at.y,key});
      if(picked){ctx.font='13px sans-serif';ctx.textAlign='left';ctx.fillStyle='#fff';ctx.fillText(npc.name,at.x+12,at.y-9);}
    }else if(object.player){ctx.fillStyle='#fff';ctx.beginPath();ctx.moveTo(at.x,at.y-7);ctx.lineTo(at.x+5,at.y);ctx.lineTo(at.x,at.y+7);ctx.lineTo(at.x-5,at.y);ctx.closePath();ctx.fill();}
    else{ctx.fillStyle=object.mob.status==='remnant'?'#f1d58f':'#b75b6c';ctx.fillRect(at.x-2,at.y-2,4,4);}
  }
  ctx.globalAlpha=1;
  const detail=$('npc-detail');detail.replaceChildren();
  const match=points.find(o=>o.npc&&npcKey(o.s,o.npc)===selectedNpc);
  if(!match){detail.textContent=selectedNpc?'선택한 NPC는 현재 필터 또는 수신 범위에 없습니다.':'지도의 점 또는 NPC 표의 상세 버튼을 선택하세요.';return;}
  const n=match.npc;
  detail.append(element('strong',`${n.name} · ${n.role_name||'이야기 NPC'}`),badge(live(match.record)?n.status:'offline'));
  detail.append(element('p',`${sourceLabel(match.s)} · 현재 ${coordinates(n.position)} → 목적지 ${coordinates(n.destination)}`));
  if(n.kind==='autonomous')detail.append(element('p',`체력 ${Math.ceil(n.health)}/${n.maximum} · 기력 ${Math.round(n.stamina)} · 소지금 ${n.gold} · 누적 수입 ${n.earned} · 사냥 ${n.kills} · 물품 ${n.goods}`));
}
function render(){
  const all=selected(), fresh=all.filter(live), stale=all.length-fresh.length;
  $('clock').textContent=new Date().toLocaleTimeString('ko-KR');
  $('connection').textContent=connected?'수집기 연결됨':'연결 끊김 · 재시도 중';
  $('connection').className='badge '+(connected?'present':'offline');
  $('instances').textContent=fresh.length;
  $('instance-note').textContent=`전체 ${all.length}개 · 수신 중단 ${stale}개`;
  const sum=k=>fresh.reduce((n,r)=>n+(r.snapshot.counts[k]||0),0);
  $('resident-count').textContent=fresh.length?sum('npc_autonomous'):'—';
  $('activity-count').textContent=fresh.length?`${sum('npc_working')} / ${sum('npc_hunting')}`:'—';
  $('recovery-count').textContent=`회복 중 ${sum('npc_recovering')}명`;
  $('resident-gold').textContent=fresh.length?sum('npc_gold').toLocaleString('ko-KR'):'—';
  $('resident-kills').textContent=fresh.length?sum('npc_kills').toLocaleString('ko-KR'):'—';
  $('npc-count').textContent=fresh.length?sum('npc_active'):'—';
  $('npc-note').textContent=`등록 ${sum('npc_registered')}명 · 실행 인스턴스별 고유 인물`;
  $('monster-count').textContent=fresh.length?sum('monsters_alive'):'—';
  $('combat-count').textContent=fresh.length?sum('monsters_combat'):'—';
  $('combat-note').textContent=`포획 잔영 ${sum('monsters_weakened')}개 별도 집계`;
  $('notice').textContent=!records.size?'게임을 실행하면 JSON 메시지를 자동으로 수신합니다. 다른 게임도 같은 수집기에 연결할 수 있습니다.':
    stale?`${stale}개 실행 인스턴스의 수신이 중단되었습니다. 위 숫자는 최신 데이터만 합산하며, 흐리게 표시된 항목은 마지막 관측값입니다.`:
    '최신 메시지 수신 중 · 몬스터 집계 범위는 각 구역에 표시됩니다. NPC의 미등장은 사망을 의미하지 않습니다.';
  $('download').disabled=!all.length;
  const regions=$('regions');regions.replaceChildren();
  const npcRows=$('npcs');npcRows.replaceChildren();
  const mobRows=$('monsters');mobRows.replaceChildren();
  const sources=$('sources');sources.replaceChildren();
  let npcMatches=0,mobCount=0;
  for(const record of all){
    const s=record.snapshot, isLive=live(record), regionNames=new Map(s.regions.map(r=>[r.id,r.name]));
    for(const region of s.regions){
      const card=element('article',undefined,'region'+(isLive?'':' stale'));
      const top=element('div',undefined,'region-top'), title=element('div');
      title.append(element('h3',region.name),element('p',`${region.kind==='village'?'마을':'탐험 구역'} · ${sourceLabel(s)}`,'source-label'));
      top.append(title,badge(isLive?region.status:'offline'));card.append(top);
      const stats=element('div',undefined,'region-stats');
      for(const [label,value] of [['NPC',region.npc_active],['몬스터',region.monsters_alive],['전투',region.monsters_combat]]){
        const item=element('span',label);item.append(element('strong',value));stats.append(item);
      }
      card.append(stats,element('p',region.npc_coverage==='complete'?'NPC 전체 집계 · 주령은 활성 구역만 관측':region.coverage==='complete'?'전체 개체 집계':'현재 불러온 개체 기준 · 미관측 구역 제외','region-note'));regions.append(card);
    }
    for(const npc of s.npcs){
      if(!npcVisible(npc))continue;
      npcMatches++;
      const row=element('tr',undefined,isLive?'':'stale');
      const nameCell=cell(row,npc.name);nameCell.append(element('small',npc.role_name||'이야기 NPC'),element('small',npc.id));
      const button=element('button','상세','npc-select');button.addEventListener('click',()=>{selectedNpc=npcKey(s,npc);renderMap(all);});nameCell.append(button);
      cell(row,sourceLabel(s));
      cell(row).append(badge(npc.status));cell(row,regionNames.get(npc.region)||'—');
      cell(row,npc.kind==='autonomous'?`${Math.ceil(npc.health)} / ${Math.round(npc.stamina)}`:'—');
      cell(row,npc.kind==='autonomous'?`${npc.gold} / ${npc.earned}`:'—');
      cell(row,npc.kind==='autonomous'?`${npc.kills} / ${npc.goods}`:'—');
      cell(row,npc.quest_target?'현재 퀘스트 대상':npc.interactable?'상호작용 가능':'—');
      const location=cell(row,coordinates(npc.position));if(npc.destination)location.append(element('small','→ '+coordinates(npc.destination)));npcRows.append(row);
    }
    for(const mob of s.monsters){
      mobCount++;
      const row=element('tr',undefined,isLive?'':'stale');
      cell(row,mob.id);cell(row,sourceLabel(s));cell(row,regionNames.get(mob.region)||'—');
      cell(row).append(badge(mob.status));cell(row,`${Math.ceil(mob.health)} / ${Math.ceil(mob.maximum)}`);mobRows.append(row);
    }
    const source=element('div',undefined,'source-row'), info=element('div');
    info.append(element('strong',sourceLabel(s)),element('small',`게임 ID ${s.game_id} · 순번 ${s.sequence} · ${Math.floor(ageOf(record)/1000)}초 전 수신`));
    info.append(element('p',s.quest?`현재 목표 · ${s.quest.title}`:'현재 목표 없음'));
    source.append(info,badge(isLive?s.mode:'offline'));sources.append(source);
  }
  if(!all.length){empty(regions,'아직 수신된 구역이 없습니다.');empty(sources,'연결된 게임의 메시지를 기다리고 있습니다.');}
  if(!npcMatches)empty(npcRows,'표시할 NPC가 없습니다.',9);
  if(!mobCount)empty(mobRows,'수신 범위에 살아 있는 몬스터가 없습니다.',5);
  $('monster-detail-count').textContent=`· ${mobCount}개 (마지막 관측 포함)`;
  renderMap(all);
}
function receive(event,data){
  if(event==='state'){
    offset=data.server_time_ms-Date.now();records.clear();
    for(const record of data.sources)records.set(keyOf(record.snapshot),record);
    updateOptions();
  }else if(event==='snapshot'){
    const key=keyOf(data.snapshot), isNew=!records.has(key);records.set(key,data);if(isNew)updateOptions();
  }else if(event==='heartbeat')offset=data.server_time_ms-Date.now();
  render();
}
async function connect(){
  const mine=++generation;controller?.abort();controller=new AbortController();
  connected=false;render();
  try{
    const response=await fetch('/api/events',{headers:token?{Authorization:'Bearer '+token}:{},signal:controller.signal,cache:'no-store'});
    if(!response.ok)throw new Error(response.status===401?'접속 토큰을 확인하세요.':`연결 오류 ${response.status}`);
    connected=true;lastStreamAt=Date.now();
    const reader=response.body.getReader(), decoder=new TextDecoder();let buffer='';
    while(true){
      const {value,done}=await reader.read();if(done)throw new Error('수집기 연결이 종료되었습니다.');
      lastStreamAt=Date.now();
      buffer+=decoder.decode(value,{stream:true});let end;
      while((end=buffer.indexOf('\n\n'))>=0){
        const block=buffer.slice(0,end);buffer=buffer.slice(end+2);
        const lines=block.split('\n'), event=lines.find(l=>l.startsWith('event: '))?.slice(7);
        const body=lines.filter(l=>l.startsWith('data: ')).map(l=>l.slice(6)).join('\n');
        if(event&&body)receive(event,JSON.parse(body));
      }
    }
  }catch(error){
    if(mine!==generation)return;
    connected=false;render();$('notice').textContent=error.message+' 2초 후 다시 연결합니다.';
    setTimeout(()=>{if(mine===generation)connect();},2000);
  }
}
$('source').addEventListener('change',render);$('search').addEventListener('input',render);$('active-only').addEventListener('change',render);
$('role-filter').addEventListener('change',render);
$('world-map').addEventListener('click',event=>{
  const canvas=event.currentTarget,rect=canvas.getBoundingClientRect();
  const x=(event.clientX-rect.left)*canvas.width/rect.width,y=(event.clientY-rect.top)*canvas.height/rect.height;
  const nearest=mapTargets.map(p=>({...p,d:Math.hypot(p.x-x,p.y-y)})).filter(p=>p.d<20).sort((a,b)=>a.d-b.d)[0];
  if(nearest){selectedNpc=nearest.key;renderMap(selected());}
});
$('auth-form').addEventListener('submit',e=>{e.preventDefault();token=$('token').value.trim();connect();});
$('download').addEventListener('click',()=>{
  const blob=new Blob([JSON.stringify({exported_at:new Date().toISOString(),sources:selected()},null,2)],{type:'application/json'});
  const url=URL.createObjectURL(blob), link=element('a');link.href=url;link.download='world-monitor-snapshot.json';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
});
setInterval(()=>{if(connected&&Date.now()-lastStreamAt>6500)controller.abort();render();},1000);connect();
