'use strict';
const $ = (s, root = document) => root.querySelector(s);
const $$ = (s, root = document) => [...root.querySelectorAll(s)];
const escapeHTML = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const storage = { get(key, fallback = '') { try { return localStorage.getItem(key) ?? fallback; } catch { return fallback; } }, set(key, value) { try { localStorage.setItem(key, value); return true; } catch { return false; } }, remove(key) { try { localStorage.removeItem(key); } catch {} } };
let token = storage.get('token'), username = storage.get('username');
let zIndex = 10, activeWindow = 'weather';
const openWindows = new Set(['weather']);
function focusWindow(id) {
 const win = document.getElementById(id); if (!win) return;
 if (activeWindow === 'snake' && id !== 'snake') pauseSnake();
 activeWindow = id; $$('.window').forEach(w => w.classList.toggle('active', w.id === id));
 win.style.zIndex = ++zIndex; renderTasks();
}
function openWindow(id) { const w = document.getElementById(id); if (!w) return; w.hidden = false; openWindows.add(id); focusWindow(id); closeStart(); }
function hideWindow(id, close = false) { document.getElementById(id).hidden = true; if (close) openWindows.delete(id); if (id === 'snake') pauseSnake(); const next = $$('.window').filter(w => !w.hidden).sort((a,b) => +b.style.zIndex - +a.style.zIndex)[0]; if (next) focusWindow(next.id); else { activeWindow = ''; renderTasks(); } }
function renderTasks() { $('#tasks').replaceChildren(); for (const id of openWindows) { const w = document.getElementById(id), b = document.createElement('button'); b.textContent = {weather:'☀ Pogoda 98',mines:'✹ Saper',snake:'▤ Wąż',notes:'▧ Notatnik',account:'⚿ Konto',about:'? O programie',computer:'▣ Mój komputer'}[id]; b.classList.toggle('active', !w.hidden && activeWindow === id); b.onclick = () => !w.hidden && activeWindow === id ? hideWindow(id) : openWindow(id); $('#tasks').append(b); } }
function closeStart() { $('#start-menu').hidden = true; $('#start-button').setAttribute('aria-expanded','false'); }
$('#start-button').onclick = () => { const visible = $('#start-menu').hidden; $('#start-menu').hidden = !visible; $('#start-button').setAttribute('aria-expanded',String(visible)); };
document.addEventListener('click', e => { const opener = e.target.closest('[data-open]'); if (opener) openWindow(opener.dataset.open); if (!e.target.closest('#start-menu,#start-button')) closeStart(); });
document.addEventListener('keydown', e => { if (e.key === 'Escape') closeStart(); });
$('#show-desktop').onclick = () => { for (const id of openWindows) hideWindow(id); closeStart(); };
$$('.window').forEach(win => {
 win.addEventListener('pointerdown', () => focusWindow(win.id));
 const bar = $('.titlebar',win);
 bar.addEventListener('dblclick', e => { if (!e.target.closest('button') && $('[data-win="max"]',win)) win.classList.toggle('maximized'); });
 $$('[data-win]',win).forEach(b => b.onclick = () => { if (b.dataset.win === 'max') win.classList.toggle('maximized'); else hideWindow(win.id,b.dataset.win === 'close'); });
 bar.addEventListener('pointerdown', e => {
  if (e.target.closest('button') || win.classList.contains('maximized') || innerWidth <= 650) return;
  const box = win.getBoundingClientRect(), dx = e.clientX - box.left, dy = e.clientY - box.top;
  bar.setPointerCapture(e.pointerId);
  const move = ev => { win.style.left = Math.max(0,Math.min(innerWidth - box.width,ev.clientX - dx))+'px'; win.style.top = Math.max(0,Math.min(innerHeight - 75,ev.clientY - dy))+'px'; };
  const end = () => { bar.removeEventListener('pointermove',move); bar.removeEventListener('pointerup',end); bar.removeEventListener('pointercancel',end); };
  bar.addEventListener('pointermove',move); bar.addEventListener('pointerup',end); bar.addEventListener('pointercancel',end);
 });
});
window.addEventListener('resize', () => { if (innerWidth <= 650) return; $$('.window').forEach(w => { const r=w.getBoundingClientRect(); if(r.left+r.width>innerWidth) w.style.left=Math.max(0,innerWidth-r.width-8)+'px'; if(r.top>innerHeight-90)w.style.top='20px'; }); });
function clock() { const now = new Date(); $('#clock').textContent = now.toLocaleTimeString('pl-PL',{hour:'2-digit',minute:'2-digit'}); $('#clock').title = now.toLocaleDateString('pl-PL',{weekday:'long',day:'numeric',month:'long',year:'numeric'}); $('#clock').dateTime = now.toISOString(); $('#network-state').title = navigator.onLine ? 'Połączenie sieciowe dostępne' : 'Brak połączenia z siecią'; }
clock(); setInterval(clock,1000);
let authMode = 'login';
function setAuthMode(mode, message = '') { authMode = mode; $$('#auth-tabs button').forEach(b => b.classList.toggle('selected',b.dataset.auth===mode)); for (const [name,visible] of Object.entries({username:mode!=='confirm',email:mode==='register',password:mode!=='confirm',code:mode==='confirm'})) { const field=$('#'+name+'-field'); field.hidden=!visible; $('input',field).required=visible; $('input',field).disabled=!visible; } $('#auth-submit').textContent={login:'Zaloguj się',register:'Utwórz konto',confirm:'Aktywuj konto'}[mode]; $('#auth-form').elements.password.autocomplete=mode==='register'?'new-password':'current-password'; $('#auth-message').textContent=message; $('#auth-message').className=''; }
$$('[data-auth]').forEach(b => b.onclick = () => setAuthMode(b.dataset.auth));
function syncAuth() { $('#signin-banner').hidden=!!token; $('#auth-tabs').hidden=!!token; $('#auth-form').hidden=!!token; $('#logged-in').hidden=!token; $('#account-label').textContent=token?username:'Sesja gościa'; }
async function request(url, {method='GET',body,signal} = {}) {
 const controller=new AbortController(), timeout=setTimeout(()=>controller.abort(),26000);
 const cancel=()=>controller.abort(); if(signal?.aborted)cancel(); signal?.addEventListener('abort',cancel,{once:true});
 try { const headers={}; if(token)headers.Authorization='Bearer '+token; if(body)headers['Content-Type']='application/json';
  const response=await fetch(url,{method,headers,body:body?JSON.stringify(body):undefined,signal:controller.signal});
  let data; try { data=await response.json(); } catch { throw new Error('Serwer zwrócił nieprawidłową odpowiedź.'); }
  if(response.status===401 && !url.includes('/login')) { expireSession(); throw new Error('Sesja wygasła. Zaloguj się ponownie.'); }
  if(!response.ok)throw new Error(data.error||'Nie udało się pobrać danych.'); return data;
 } catch(e) { if(e.name==='AbortError') { if(signal?.aborted)throw e; throw new Error('Przekroczono czas oczekiwania. Spróbuj ponownie.'); } if(e instanceof TypeError)throw new Error('Brak połączenia z serwerem. Sprawdź sieć i spróbuj ponownie.'); throw e; }
 finally { clearTimeout(timeout); signal?.removeEventListener('abort',cancel); }
}
function expireSession() { token=''; storage.remove('token'); clearWeather(); syncAuth(); setAuthMode('login','Sesja wygasła. Zaloguj się ponownie.'); openWindow('account'); }
$('#auth-form').onsubmit=async e=> { e.preventDefault(); const b=$('#auth-submit'); b.disabled=true; $('#auth-message').textContent='Łączenie…'; const form=e.currentTarget, mode=authMode;
 try {
  if(mode==='login') { const data=await request('/api/login',{method:'POST',body:{username:form.elements.username.value.trim(),password:form.elements.password.value}}); token=data.token; username=form.elements.username.value.trim(); storage.set('token',token); storage.set('username',username); form.elements.password.value=''; syncAuth(); hideWindow('account',true); openWindow('weather'); fetchWeather(); }
  else if(mode==='register') { const data=await request('/api/register',{method:'POST',body:{username:form.elements.username.value.trim(),email:form.elements.email.value.trim(),password:form.elements.password.value}}); setAuthMode('confirm','Konto lokalne — kod aktywacji: '+data.confirm_code+'\nWpisaliśmy go poniżej. E-mail nie jest wysyłany.'); form.elements.code.value=data.confirm_code; }
  else { await request('/api/confirm',{method:'POST',body:{code:form.elements.code.value.trim()}}); setAuthMode('login','Konto aktywne. Możesz się zalogować.'); $('#auth-message').className='success'; }
 } catch(err) { $('#auth-message').textContent=err.message; $('#auth-message').className='error'; } finally { b.disabled=false; }
};
$('#logout').onclick=async()=>{ const b=$('#logout'); b.disabled=true; try { await request('/api/logout',{method:'POST'}); token=''; username=''; storage.remove('token'); storage.remove('username'); clearWeather(); syncAuth(); setAuthMode('login'); } catch(e) { $('#logged-in p').textContent=e.message; } finally { b.disabled=false; } };
let locationData={name:'Warszawa',country:'Polska',lat:52.23,lon:21.01,timezone:'Europe/Warsaw'};
try { const saved=JSON.parse(storage.get('weather-location','null')); if(saved && typeof saved.name==='string' && Number.isFinite(saved.lat)&&Number.isFinite(saved.lon)&&Math.abs(saved.lat)<=90&&Math.abs(saved.lon)<=180)locationData=saved; } catch {}
let generation=0, weatherController=null, searchController=null, searchSequence=0;
const sources=['openmeteo','wttr','7timer'], sourceNames={openmeteo:'Open-Meteo',wttr:'wttr.in','7timer':'7Timer!'};
let states={};
const fmt=(value,suffix='')=>typeof value==='number'&&Number.isFinite(value)?Math.round(value)+suffix:'—';
function weatherDescription(code) { if(code===null||code===undefined)return 'Brak opisu'; if(code===0)return 'Bezchmurnie'; if(code<=2)return 'Częściowe zachmurzenie'; if(code===3)return 'Pochmurno'; if(code<=48)return 'Mgła'; if(code<=57)return 'Mżawka'; if(code<=67)return 'Deszcz'; if(code<=77)return 'Śnieg'; if(code<=82)return 'Przelotny deszcz'; if(code<=86)return 'Przelotny śnieg'; return 'Burza'; }
const weatherSymbol=code=>code==null?'?':code===0?'☀':code<=3?'☁':code<=48?'≋':code<=67?'☂':code<=77?'❄':code<=82?'☂':code<=86?'❄':'ϟ';
function locationLabel() { $('#city-input').value=locationData.name; $('#location-title').textContent=[locationData.name,locationData.country].filter(Boolean).join(', '); $('#location-meta').textContent=`${Math.abs(locationData.lat).toFixed(2)}° ${locationData.lat<0?'S':'N'} · ${Math.abs(locationData.lon).toFixed(2)}° ${locationData.lon<0?'W':'E'} · ${locationData.timezone||'UTC'}`; }
function clearWeather() { ++generation; weatherController?.abort(); searchController?.abort(); ++searchSequence; states={}; $('#search-results').hidden=true; renderWeather(); }
function renderSources() { $('#source-cards').innerHTML=sources.map(id=>{const s=states[id]||{},data=s.data; let note='Oczekuje na połączenie'; if(s.status==='loading')note='Pobieranie danych…'; else if(s.status==='error')note=s.error; else if(data)note=id==='7timer'?'Prognoza co 3 godziny':data.time?'Dane: '+data.time:'Dane bieżące'; return `<article class="source-card"><h3><span class="dot ${s.status||''}"></span>${sourceNames[id]}</h3><p>${escapeHTML(note)}</p>${data?`<b>${id==='7timer'?data.forecast.length+' terminów':fmt(data.temperature_c,'°C')}</b>`:''}${s.status==='error'?`<button data-retry="${id}">Ponów połączenie</button>`:''}</article>`;}).join(''); }
function renderWeather() {
 renderSources(); const success=sources.filter(s=>states[s]?.status==='ok').length, loading=sources.some(s=>states[s]?.status==='loading');
 $('#connection').textContent=loading?'ŁĄCZENIE…':success?`${success} / 3 ŹRÓDŁA`:'BRAK DANYCH'; $('#connection').classList.toggle('good',success>0&&!loading);
 $('#weather-status').textContent=!token?'Zaloguj się, aby pobrać pogodę.':loading?'Pobieranie pogody — gotowe źródła są już widoczne.':success===3?'Gotowy. Odebrano dane ze wszystkich źródeł.':success?'Część źródeł jest niedostępna. Możesz ponowić połączenie.':'Nie pobrano danych. Wybierz miasto lub ponów połączenie.';
 const main=states.openmeteo?.data||states.wttr?.data;
 if(main) { const description=main.source==='Open-Meteo'?weatherDescription(main.weather_code):main.description||'Warunki bieżące'; $('#current-weather').innerHTML=`<div class="weather-main"><span class="day-symbol" style="font-size:58px" aria-hidden="true">${main.source==='Open-Meteo'?weatherSymbol(main.weather_code):'☁'}</span><div class="temperature">${fmt(main.temperature_c,'°')}</div><div><p class="weather-description">${escapeHTML(description)}</p><p class="weather-meta">${escapeHTML(main.source)} · °C<br>${escapeHTML(main.time||'Czas pomiaru niedostępny')}</p></div></div><div class="weather-metrics"><div><small>Odczuwalna</small><b>${fmt(main.feels_like_c,'°C')}</b></div><div><small>Wiatr</small><b>${fmt(main.wind_speed_kmh,' km/h')}</b></div><div><small>Wilgotność</small><b>${fmt(main.humidity_percent,'%')}</b></div><div><small>Ciśnienie</small><b>${fmt(main.pressure_mb,' hPa')}</b></div></div>`; }
 else $('#current-weather').innerHTML=`<div class="empty-state"><span class="pixel-icon weather large"></span><h3>${!token?'Twoja prognoza czeka':loading?'Łączymy się ze stacjami…':'Brak bieżących danych'}</h3><p>${!token?'Zaloguj się, aby zobaczyć pogodę.':loading?'Każdy serwis odpowiada niezależnie.':'Sprawdź połączenie i spróbuj ponownie.<br>Dostępna prognoza może być w zakładce obok.'}</p><button ${!token?'data-open="account"':'data-refresh'}>${!token?'Otwórz logowanie':'Odśwież'}</button></div>`;
 const daily=states.openmeteo?.data?.daily;
 $('#daily-forecast').innerHTML=daily?.time?.length?daily.time.map((day,i)=>`<div class="day"><strong>${escapeHTML(new Date(day+'T12:00:00').toLocaleDateString('pl-PL',{weekday:'short'}))}</strong><small>${escapeHTML(day.slice(5).split('-').reverse().join('.'))}</small><span class="day-symbol" title="${weatherDescription(daily.weather_code?.[i])}">${weatherSymbol(daily.weather_code?.[i])}</span><b>${fmt(daily.temperature_2m_max?.[i],'°')}</b><small>${fmt(daily.temperature_2m_min?.[i],'°')} noc</small><small>Opady ${fmt(daily.precipitation_probability_max?.[i],'%')}</small></div>`).join(''):`<p style="padding:12px">${states.openmeteo?.status==='loading'?'Pobieranie prognozy…':'Prognoza dzienna jest niedostępna.'}</p>`;
 renderHourly();
}
function renderHourly() { const data=states['7timer']?.data; const target=$('#hourly-forecast'); if(!data){target.innerHTML='<p>'+ (states['7timer']?.status==='loading'?'Pobieranie prognozy…':'Prognoza 7Timer jest niedostępna.')+'</p>';return;} const init=String(data.init_time); if(!/^\d{10}$/.test(init)){target.textContent='Źródło zwróciło nieprawidłową datę prognozy.';return;} const base=Date.UTC(+init.slice(0,4),+init.slice(4,6)-1,+init.slice(6,8),+init.slice(8,10));
 const future=data.forecast.filter(x=>typeof x.timepoint_h==='number'&&base+x.timepoint_h*3600000>=Date.now());
 const names={clear:'Bezchmurnie',pcloudy:'Małe zachmurzenie',mcloudy:'Duże zachmurzenie',cloudy:'Pochmurno',humid:'Mglisto',lightrain:'Lekki deszcz',oshower:'Przelotne opady',ishower:'Przelotne opady',lightsnow:'Lekki śnieg',rain:'Deszcz',snow:'Śnieg',rainsnow:'Deszcz ze śniegiem',ts:'Burza',tsrain:'Burza z deszczem',windy:'Wietrznie'};
 const winds=['—','< 1','1–12','12–29','29–39','39–62','62–88','88–117','> 117'];
 if(!future.length){target.textContent='Dostawca zwrócił nieaktualną prognozę. Spróbuj później.';return;}
 target.innerHTML=`<table class="forecast-table"><thead><tr><th>Termin lokalny</th><th>Pogoda</th><th>°C</th><th>Wiatr km/h</th></tr></thead><tbody>${future.slice(0,8).map(e=>`<tr><td>${escapeHTML(new Date(base+e.timepoint_h*3600000).toLocaleString('pl-PL',{timeZone:locationData.timezone||'UTC',day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit'}))}</td><td>${escapeHTML(names[e.weather.replace(/(day|night)$/,'')]||e.weather)}</td><td>${fmt(e.temperature_c,'°')}</td><td>${winds[e.wind_speed_category]||'—'}</td></tr>`).join('')}</tbody></table><p class="muted">Czas lokalny: ${escapeHTML(locationData.timezone||'UTC')}. Wiatr: przedziały dostawcy.</p>`;
}
async function loadSource(id,run,signal) { states[id]={status:'loading'};renderWeather(); const params=new URLSearchParams({lat:locationData.lat,lon:locationData.lon,city:locationData.name}); try { const data=await request('/api/weather/'+id+'?'+params,{signal}); if(run!==generation)return; states[id]={status:'ok',data}; } catch(e) { if(run!==generation||e.name==='AbortError')return; states[id]={status:'error',error:e.message}; } if(run===generation)renderWeather(); }
function fetchWeather() { if(!token){openWindow('account');return;} weatherController?.abort(); weatherController=new AbortController(); const run=++generation; states={}; sources.forEach(id=>states[id]={status:'loading'}); renderWeather(); sources.forEach(id=>loadSource(id,run,weatherController.signal)); }
document.addEventListener('click',e=>{const b=e.target.closest('[data-retry]');if(b&&token)loadSource(b.dataset.retry,generation,weatherController.signal);if(e.target.closest('[data-refresh]'))fetchWeather();});
$('#refresh').onclick=fetchWeather; $('#menu-refresh').onclick=fetchWeather;
$('#city-form').onsubmit=async e=>{e.preventDefault();if(!token){openWindow('account');return;}const name=$('#city-input').value.trim(); if(name.length<2)return; searchController?.abort();searchController=new AbortController();const run=++searchSequence, signal=searchController.signal; const box=$('#search-results');box.hidden=false;box.textContent='Wyszukiwanie miejscowości…';try {const data=await request('/api/locations?name='+encodeURIComponent(name),{signal});if(run!==searchSequence)return;box.replaceChildren();if(!data.results.length){box.textContent='Nie znaleziono miejscowości. Sprawdź pisownię lub wpisz pobliskie miasto.';return;}data.results.forEach(city=>{const b=document.createElement('button');b.type='button';b.textContent=[city.name,city.region,city.country].filter(Boolean).join(', ');b.onclick=()=>{locationData=city;storage.set('weather-location',JSON.stringify(city));box.hidden=true;locationLabel();fetchWeather();};box.append(b);});}catch(err){if(run===searchSequence&&err.name!=='AbortError')box.textContent=err.message;}};
$$('[data-tab]').forEach(b=>b.onclick=()=>{$$('[data-tab]').forEach(t=>t.setAttribute('aria-selected',String(t===b)));$$('[role=tabpanel]').forEach(p=>p.hidden=p.id!==b.dataset.tab);});
window.addEventListener('offline',()=>{$('#weather-status').textContent='Brak sieci. Widoczne dane pochodzą z ostatniego pobrania.';});
window.addEventListener('online',()=>{clock();if(token)fetchWeather();});
// Notes never leave this browser unless the user downloads a file.
$('#note-text').value=storage.get('pogoda98-notes');
$('#note-text').oninput=()=>{$('#note-status').textContent=storage.set('pogoda98-notes',$('#note-text').value)?'Zapisano w tej przeglądarce':'Zapis lokalny niedostępny. Użyj „Zapisz jako plik”.';};
$('#save-notes').onclick=()=>{const url=URL.createObjectURL(new Blob([$('#note-text').value],{type:'text/plain;charset=utf-8'}));const a=document.createElement('a');a.href=url;a.download='Notatki.txt';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
// Minesweeper: safe opening, flood reveal, flags and a real win condition.
let cells=[],minesStarted=false,minesEnded=false,mineStart=0,mineElapsed=0;
function neighbors(index){const x=index%9,y=Math.floor(index/9),out=[];for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++){if(!dx&&!dy)continue;const xx=x+dx,yy=y+dy;if(xx>=0&&xx<9&&yy>=0&&yy<9)out.push(yy*9+xx);}return out;}
function newMines(){cells=Array.from({length:81},()=>({mine:false,open:false,flag:false,n:0}));minesStarted=false;minesEnded=false;mineElapsed=0;$('#mine-time').textContent='000';$('#mine-status').textContent='10 min. Pierwsze pole jest bezpieczne.';$('#mine-face').textContent='☺';renderMines();}
function seedMines(first){const safe=new Set([first,...neighbors(first)]),pool=cells.map((_,i)=>i).filter(i=>!safe.has(i));for(let i=pool.length-1;i>0;i--){const j=Math.floor(Math.random()*(i+1));[pool[i],pool[j]]=[pool[j],pool[i]];}pool.slice(0,10).forEach(i=>cells[i].mine=true);cells.forEach((c,i)=>c.n=neighbors(i).filter(n=>cells[n].mine).length);minesStarted=true;mineStart=Date.now();}
function revealCell(i){if(minesEnded||cells[i].flag||cells[i].open)return;if(!minesStarted)seedMines(i);if(cells[i].mine){cells[i].open=true;minesEnded=true;$('#mine-status').textContent='Trafiona mina. Spróbuj jeszcze raz!';$('#mine-face').textContent='☹';}else{const queue=[i];while(queue.length){const k=queue.pop(),c=cells[k];if(c.open||c.flag||c.mine)continue;c.open=true;if(!c.n)queue.push(...neighbors(k));}if(cells.filter(c=>c.open).length===71){minesEnded=true;cells.forEach(c=>{if(c.mine)c.flag=true;});$('#mine-status').textContent='Wygrana! Wszystkie bezpieczne pola odkryte.';$('#mine-face').textContent='☻';}else $('#mine-status').textContent='Odkryto '+cells.filter(c=>c.open).length+' / 71 bezpiecznych pól.';}renderMines();}
function flagCell(i){if(minesEnded||cells[i].open)return;cells[i].flag=!cells[i].flag;renderMines();}
function renderMines(){const board=$('#mine-board');board.replaceChildren();cells.forEach((c,i)=>{const b=document.createElement('button');b.type='button';const shown=c.open||(minesEnded&&c.mine);b.classList.toggle('revealed',shown);b.classList.toggle('exploded',c.open&&c.mine);b.dataset.n=c.n;b.textContent=shown?(c.mine?'✹':c.n||''):c.flag?'⚑':'';b.setAttribute('aria-label',`Pole ${Math.floor(i/9)+1}, ${i%9+1}: ${shown?(c.mine?'mina':c.n+' min w pobliżu'):c.flag?'flaga':'zakryte'}`);b.onclick=()=>$('#flag-mode').checked?flagCell(i):revealCell(i);b.oncontextmenu=e=>{e.preventDefault();flagCell(i);};board.append(b);});$('#mine-count').textContent=String(10-cells.filter(c=>c.flag).length).padStart(3,'0');}
$('#mines-new').onclick=newMines;$('#mine-face').onclick=newMines;
setInterval(()=>{if(minesStarted&&!minesEnded){mineElapsed=Math.min(999,Math.floor((Date.now()-mineStart)/1000));$('#mine-time').textContent=String(mineElapsed).padStart(3,'0');}},1000);
// Snake pauses whenever it loses focus, is minimized, or the page is hidden.
let snake=[],direction={x:1,y:0},queuedDirection=null,food=null,snakeRunning=false,snakeEnded=false,snakeScore=0;
let snakeBest=Math.max(0,Number(storage.get('pogoda98-snake-best','0'))||0);
const canvas=$('#snake-board'),ctx=canvas.getContext('2d');
function spawnFood(){const empty=[];for(let y=0;y<20;y++)for(let x=0;x<20;x++)if(!snake.some(s=>s.x===x&&s.y===y))empty.push({x,y});return empty[Math.floor(Math.random()*empty.length)]||null;}
function newSnake(){snake=[{x:7,y:10},{x:6,y:10},{x:5,y:10}];direction={x:1,y:0};queuedDirection=null;snakeScore=0;snakeRunning=false;snakeEnded=false;food=spawnFood();$('#snake-status').textContent='Naciśnij Start. Sterowanie: strzałki / WASD.';drawSnake();}
function drawSnake(){ctx.fillStyle='#abb781';ctx.fillRect(0,0,300,300);ctx.fillStyle='#a2ad79';for(let y=0;y<20;y++)for(let x=0;x<20;x++)ctx.fillRect(x*15+6,y*15+6,2,2);ctx.fillStyle='#27351d';snake.forEach((s,i)=>{ctx.fillRect(s.x*15+1,s.y*15+1,13,13);if(i===0){ctx.fillStyle='#d2dfa6';ctx.fillRect(s.x*15+4,s.y*15+4,3,3);ctx.fillStyle='#27351d';}});if(food){ctx.fillStyle='#66391f';ctx.fillRect(food.x*15+3,food.y*15+3,9,9);}$('#snake-score').textContent=snakeScore;$('#snake-best').textContent=snakeBest;}
function pauseSnake(){if(snakeRunning){snakeRunning=false;$('#snake-status').textContent='Pauza. Naciśnij Start lub spację, aby wznowić.';}}
function toggleSnake(){if(snakeEnded)newSnake();if(snakeRunning)pauseSnake();else{snakeRunning=true;$('#snake-status').textContent='Zbieraj jedzenie. Uważaj na ściany i ogon!';canvas.focus();}}
function steer(name){const d={up:{x:0,y:-1},down:{x:0,y:1},left:{x:-1,y:0},right:{x:1,y:0}}[name];if(!d||queuedDirection||!snakeRunning)return;if(d.x===-direction.x&&d.y===-direction.y)return;queuedDirection=d;}
function tickSnake(){if(!snakeRunning)return;if(queuedDirection){direction=queuedDirection;queuedDirection=null;}const head={x:snake[0].x+direction.x,y:snake[0].y+direction.y};const eat=food&&head.x===food.x&&head.y===food.y;const body=eat?snake:snake.slice(0,-1);if(head.x<0||head.x>=20||head.y<0||head.y>=20||body.some(s=>s.x===head.x&&s.y===head.y)){snakeRunning=false;snakeEnded=true;$('#snake-status').textContent='Koniec gry! Wynik: '+snakeScore+'. Naciśnij Nowa gra.';return;}snake.unshift(head);if(eat){snakeScore+=10;if(snakeScore>snakeBest){snakeBest=snakeScore;storage.set('pogoda98-snake-best',snakeBest);}food=spawnFood();if(!food){snakeRunning=false;snakeEnded=true;$('#snake-status').textContent='Wygrana! Cała plansza jest Twoja.';}}else snake.pop();drawSnake();}
$('#snake-new').onclick=newSnake;$('#snake-pause').onclick=toggleSnake;
$$('[data-dir]').forEach(b=>b.onclick=()=>{if(!snakeRunning&&!snakeEnded)toggleSnake();steer(b.dataset.dir);});
document.addEventListener('keydown',e=>{if(activeWindow!=='snake'||$('#snake').hidden||e.target.matches('input,textarea'))return;const name={ArrowUp:'up',w:'up',ArrowDown:'down',s:'down',ArrowLeft:'left',a:'left',ArrowRight:'right',d:'right'}[e.key];if(name){e.preventDefault();steer(name);}if(e.code==='Space'&&!e.target.matches('button')){e.preventDefault();toggleSnake();}});
document.addEventListener('visibilitychange',()=>{if(document.hidden)pauseSnake();});window.addEventListener('blur',pauseSnake);setInterval(tickSnake,140);
setAuthMode('login');syncAuth();locationLabel();newMines();newSnake();focusWindow('weather');renderWeather();if(token)fetchWeather();
