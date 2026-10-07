// Run against an isolated collector, never against a live game's collector.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

(async () => {
  const browser = await chromium.launch({headless:true, executablePath:process.env.MONITOR_BROWSER || 'C:/Program Files/Google/Chrome/Application/chrome.exe'});
  try {
    const page = await browser.newPage({viewport:{width:1440,height:1080}});
    const errors=[];
    page.on('pageerror',e=>errors.push(e.message));
    const base=process.env.MONITOR_TEST_URL || 'http://127.0.0.1:18766';
    await page.goto(base,{waitUntil:'domcontentloaded'});
    await page.waitForFunction(()=>document.getElementById('connection').textContent==='수집기 연결됨');
    const snapshot=JSON.parse(fs.readFileSync(path.join(__dirname,'snapshot.example.json'),'utf8'));
    snapshot.timestamp_ms=Date.now();snapshot.session='browser-test-'+Date.now();
    const second=structuredClone(snapshot);
    second.game_id='second-sample';second.game_name='두 번째 게임 (샘플)';
    async function send(data){
      data.sequence++;data.timestamp_ms=Date.now();
      const response=await page.request.post(base+'/api/snapshots',{data});
      assert.equal(response.status(),202);
    }
    await send(snapshot);await send(second);
    await page.waitForFunction(()=>document.getElementById('instances').textContent==='2');
    assert.equal(await page.locator('#npc-count').textContent(),'2');
    assert.equal(await page.locator('#regions .region').count(),2);
    assert.equal(await page.locator('#npcs tr').count(),2);
    await page.screenshot({path:path.join(__dirname,'preview.png'),fullPage:true});
    await page.selectOption('#source','game:sample-game');
    assert.equal(await page.locator('#npc-count').textContent(),'1');
    await page.fill('#search','없는이름');
    assert.match(await page.locator('#npcs').textContent(),/표시할 NPC가 없습니다/);
    await page.fill('#search','');await page.selectOption('#source','');
    snapshot.npcs[0].name='<img src=x onerror=alert(1)>';
    await send(snapshot);
    await page.waitForFunction(()=>document.getElementById('npcs').textContent.includes('<img'));
    assert.equal(await page.locator('#npcs img').count(),0);
    snapshot.npcs[0].name='샘플 NPC';await send(snapshot);await send(second);
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
    await page.screenshot({path:path.join(__dirname,'preview-mobile.png'),fullPage:true});
    await page.waitForFunction(()=>document.getElementById('instances').textContent==='0',{},{timeout:8000});
    assert.equal(await page.locator('#npc-count').textContent(),'—');
    assert.match(await page.locator('#notice').textContent(),/수신이 중단/);
    await send(snapshot);
    await page.waitForFunction(()=>document.getElementById('instances').textContent==='1');
    assert.deepEqual(errors,[]);
    console.log('PASS: two games, aggregate, filter, safe text, mobile layout, stale exclusion, recovery; no page errors.');
  } finally {
    await browser.close();
  }
})().catch(e=>{console.error(e);process.exitCode=1;});
