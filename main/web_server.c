#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_http_server.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "cJSON.h"

#include "app_state.h"
#include "web_server.h"

static httpd_handle_t s_httpd;

static const char s_index_html[] =
"<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>RTable</title><style>"
"body{font-family:Verdana,sans-serif;max-width:1020px;margin:20px auto;padding:0 12px;background:#f3f6f7;color:#1a252c}"
"h1{margin:0 0 10px}.status{font-size:12px;color:#4a5a62;margin-bottom:10px}"
".tabs{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px}"
".tab{padding:9px 12px;background:#dce7ec;border-radius:8px;cursor:pointer;border:none}"
".tab.a{background:#186a8a;color:#fff}"
".pane{display:none}.pane.a{display:block}"
"fieldset{margin:10px 0;padding:12px;border:1px solid #cad4da;background:#fff;border-radius:8px}"
"label{display:block;margin:8px 0 3px}input,select,button{padding:8px;border-radius:6px;border:1px solid #9fb0b9}"
"button{background:#186a8a;color:#fff;border:none;cursor:pointer;margin-right:8px}button.d{background:#8a2d18}.kpi{font-weight:bold}"
".row{display:flex;gap:10px;flex-wrap:wrap}.row>*{flex:1;min-width:220px}"
".line{display:flex;gap:8px;align-items:end;flex-wrap:wrap}.line>*{min-width:200px}"
".err{color:#9a2d2d;font-size:12px;min-height:16px}"
".modal{position:fixed;inset:0;background:rgba(15,25,35,.45);display:none;align-items:center;justify-content:center;z-index:10}"
".modal.a{display:flex}.modal-box{background:#fff;border-radius:10px;padding:16px;max-width:360px;width:90%;box-shadow:0 8px 30px rgba(0,0,0,.2)}"
".modal-actions{margin-top:12px;display:flex;gap:8px;justify-content:flex-end}"
"</style></head><body>"
"<h1>Rotating Table Control</h1><div id='status' class='status'>Loading...</div>"
"<div class='tabs'>"
"<button class='tab a' data-pane='p1' onclick='openTab(event)'>Control</button>"
"<button class='tab' data-pane='p2' onclick='openTab(event)'>Profiles & Params</button>"
"<button class='tab' data-pane='p3' onclick='openTab(event)'>Wi-Fi</button>"
"<button class='tab' data-pane='p4' onclick='openTab(event)'>About</button>"
"</div>"

"<div id='p1' class='pane a'>"
"<fieldset><legend>Main Control</legend>"
"<div class='row'><div><label>Profile</label><div id='active_name' class='kpi'>-</div></div>"
"<div><label>Rotation direction</label><div id='rot_dir_kpi' class='kpi'>CW</div></div>"
"<div><label>Table rotation angle</label><div id='table_angle' class='kpi'>0.0 deg</div></div>"
"<div><label>Tilt angle</label><div id='tilt_angle' class='kpi'>0.0 deg</div></div></div>"
"<button id='startStopBtn' onclick='toggleStartStop()'>Start</button>"
"<button class='d' onclick='goHome()'>Reset (Home 0/0)</button>"
"</fieldset></div>"

"<div id='p2' class='pane'>"
"<fieldset><legend>Profile & Motion Parameters</legend>"
"<div class='line'><div style='flex:1'><label>Profile select</label><select id='profile_select'></select></div>"
"<div style='flex:1'><label>Profile name</label><input id='profile_name_combo' list='profile_list' placeholder='Type name'></div>"
"<button onclick='addProfile()'>Add</button></div><datalist id='profile_list'></datalist>"
"<div class='row'><div><label>Rotation direction</label><select id='rotation_dir'><option value='1'>CW</option><option value='-1'>CCW</option></select></div>"
"<div><label>RPM</label><input id='rpm' type='number' step='0.1'></div>"
"<div><label>Turn/Grad (deg per turn)</label><input id='turn_grad' type='number' step='0.1'></div></div>"
"<div class='row'><div><label>Min angle</label><input id='min_angle' type='number' step='1'></div>"
"<div><label>Max angle</label><input id='max_angle' type='number' step='1'></div></div>"
"<div style='margin-top:10px'><button onclick='saveProfile()'>Save</button><button class='d' onclick='deleteProfile()'>Delete</button><button onclick='activateProfile()'>Activate</button></div>"
"<div id='p2_err' class='err'></div>"
"</fieldset></div>"

"<div id='p3' class='pane'>"
"<fieldset><legend>Wi-Fi</legend><div class='row'><div><label>Mode</label><select id='wifi_mode'><option value='0'>STA with AP fallback</option><option value='1'>AP only</option></select></div>"
"<div><label>STA SSID</label><input id='sta_ssid'></div><div><label>STA PASS</label><input id='sta_pass' type='password'></div></div>"
"<div class='row'><div><label>AP SSID</label><input id='ap_ssid'></div><div><label>AP PASS</label><input id='ap_pass' type='password'></div></div>"
"<button onclick='saveWifi()'>Save Wi-Fi</button></fieldset>"
"</div>"

"<div id='p4' class='pane'>"
"<fieldset><legend>About</legend><div><b>Name:</b> Rotating Table Controller</div><div><b>Version:</b> 1.0.0</div><br>"
"<button class='d' onclick='resetSettingsConfirm()'>Reset settings</button></fieldset>"
"</div>"
"<div id='unsavedModal' class='modal'><div class='modal-box'><div><b>Unsaved changes</b></div><div style='margin-top:6px'>Save changes before switching profile?</div><div class='modal-actions'><button id='mSave' onclick='resolveUnsavedModal(\"save\")'>Save</button><button onclick='resolveUnsavedModal(\"discard\")'>Discard</button><button class='d' onclick='resolveUnsavedModal(\"cancel\")'>Cancel</button></div></div></div>"

"<script>"
"let lastStatus={},profilesCache=[],selectedProfileIndex=-1,dirty=false,lockInputs=false,unsavedResolver=null;"
"const eps=0.0001;"
"function openTab(e){document.querySelectorAll('.tab').forEach(t=>t.classList.remove('a'));document.querySelectorAll('.pane').forEach(p=>p.classList.remove('a'));e.target.classList.add('a');document.getElementById(e.target.dataset.pane).classList.add('a');}"
"async function api(path,m='GET',b){const r=await fetch(path,{method:m,headers:{'Content-Type':'application/json'},body:b?JSON.stringify(b):undefined});if(!r.ok)throw new Error(await r.text());return r.json().catch(()=>({}));}"
"function val(id){return document.getElementById(id).value}"
"function set(id,v){document.getElementById(id).value=v}"
"function setErr(m){document.getElementById('p2_err').textContent=m||'';}"
"function profName(s){const i=s.active_profile||0;return (s.profiles&&s.profiles[i])?s.profiles[i].name:'-';}"
"function fillDatalist(){const dl=document.getElementById('profile_list');dl.innerHTML='';profilesCache.forEach(p=>{const o=document.createElement('option');o.value=p.name;dl.appendChild(o);});}"
"function fillProfileSelect(){const s=document.getElementById('profile_select');s.innerHTML='';profilesCache.forEach((p,i)=>{const o=document.createElement('option');o.value=i;o.textContent=i+': '+(p.name||'(empty)');s.appendChild(o);});if(selectedProfileIndex>=0&&selectedProfileIndex<profilesCache.length){s.value=String(selectedProfileIndex);} }"
"function editorData(){return {name:val('profile_name_combo').trim(),rotation_dir:+val('rotation_dir'),rpm:+val('rpm'),turn_grad:+val('turn_grad'),min_angle:+val('min_angle'),max_angle:+val('max_angle')};}"
"function profileEq(p,d){return p&&((p.rotation_dir||1)===(d.rotation_dir||1))&&Math.abs(p.rpm-d.rpm)<eps&&Math.abs(p.turn_grad-d.turn_grad)<eps&&Math.abs(p.min_angle-d.min_angle)<eps&&Math.abs(p.max_angle-d.max_angle)<eps&&((p.name||'')===d.name);}"
"function updateDirty(){if(lockInputs||selectedProfileIndex<0||selectedProfileIndex>=profilesCache.length)return;dirty=!profileEq(profilesCache[selectedProfileIndex],editorData());}"
"function loadEditor(idx){if(idx<0||idx>=profilesCache.length)return;lockInputs=true;const p=profilesCache[idx];selectedProfileIndex=idx;set('profile_name_combo',p.name||'');set('rotation_dir',(p.rotation_dir||1));set('rpm',p.rpm);set('turn_grad',p.turn_grad);set('min_angle',p.min_angle);set('max_angle',p.max_angle);fillProfileSelect();dirty=false;lockInputs=false;setErr('');}"
"function showUnsavedModal(){return new Promise(resolve=>{unsavedResolver=resolve;document.getElementById('unsavedModal').classList.add('a');});}"
"function resolveUnsavedModal(action){document.getElementById('unsavedModal').classList.remove('a');if(unsavedResolver){const r=unsavedResolver;unsavedResolver=null;r(action);}}"
"function findByName(name){return profilesCache.findIndex(p=>(p.name||'')===name);}"
"async function saveProfileCore(showAlert){setErr('');if(selectedProfileIndex<0){setErr('No selected profile');return false;}const d=editorData();if(!d.name){setErr('Profile name must not be empty');return false;}const dup=profilesCache.findIndex((p,i)=>i!==selectedProfileIndex&&(p.name||'')===d.name);if(dup>=0){setErr('Profile name must be unique');return false;}try{await api('/api/profiles','POST',{op:'save',index:selectedProfileIndex,name:d.name,rotation_dir:d.rotation_dir,rpm:d.rpm,turn_grad:d.turn_grad,min_angle:d.min_angle,max_angle:d.max_angle});await refresh(true);loadEditor(selectedProfileIndex);if(showAlert){};return true;}catch(e){setErr(String(e.message||e));return false;}}"
"async function maybeSwitchProfile(newIdx){if(newIdx===selectedProfileIndex||newIdx<0)return;updateDirty();if(dirty){const a=await showUnsavedModal();if(a==='cancel'){set('profile_name_combo',profilesCache[selectedProfileIndex]?.name||'');return;}if(a==='save'){const ok=await saveProfileCore(false);if(!ok){set('profile_name_combo',profilesCache[selectedProfileIndex]?.name||'');return;}}}loadEditor(newIdx);}"
"async function onProfileNameChanged(){const idx=findByName(val('profile_name_combo'));if(idx>=0){await maybeSwitchProfile(idx);}else{updateDirty();}}"
"async function onProfileSelectChanged(){const idx=+val('profile_select');if(Number.isFinite(idx)){await maybeSwitchProfile(idx);} }"
"async function refresh(force){const s=await api('/api/status');lastStatus=s;profilesCache=s.profiles||[];fillDatalist();"
"fillProfileSelect();"
"set('wifi_mode',s.wifi_mode);set('sta_ssid',s.sta_ssid);set('sta_pass',s.sta_pass);set('ap_ssid',s.ap_ssid);set('ap_pass',s.ap_pass);"
"document.getElementById('active_name').textContent=profName(s);"
"document.getElementById('rot_dir_kpi').textContent=((s.rotation_dir||1)>=0)?'CW':'CCW';"
"document.getElementById('table_angle').textContent=(s.table_angle_deg||0).toFixed(1)+' deg';"
"document.getElementById('tilt_angle').textContent=(s.current_angle||0).toFixed(1)+' deg';"
"document.getElementById('startStopBtn').textContent=s.power_on?'Stop':'Start';"
"document.getElementById('status').textContent='Power: '+(s.power_on?'ON':'OFF')+' | Homing: '+(s.home_requested?'YES':'NO')+' | Active: '+profName(s);"
"if(force||selectedProfileIndex<0||selectedProfileIndex>=profilesCache.length){loadEditor((s.active_profile>=0)?s.active_profile:0);}else{updateDirty();if(!dirty){loadEditor(selectedProfileIndex);}}}"
"async function toggleStartStop(){if(lastStatus.power_on){await api('/api/stop','POST',{});}else{await api('/api/start','POST',{});}refresh(true);}"
"async function goHome(){await api('/api/home','POST',{});refresh(true);}"
"async function addProfile(){await api('/api/profiles','POST',{op:'create',name:''});await refresh(true);loadEditor((profilesCache.length||1)-1);setErr('New profile created. Enter name and Save.');}"
"async function saveProfile(){await saveProfileCore(true);}"
"async function deleteProfile(){setErr('');if(selectedProfileIndex<0)return;if(!confirm('Delete selected profile?'))return;try{await api('/api/profiles','POST',{op:'delete',index:selectedProfileIndex});await refresh(true);}catch(e){setErr(String(e.message||e));}}"
"async function activateProfile(){if(selectedProfileIndex<0)return;await api('/api/profiles','POST',{op:'activate',index:selectedProfileIndex});await refresh(true);}"
"async function saveWifi(){await api('/api/config','POST',{wifi_mode:+val('wifi_mode'),sta_ssid:val('sta_ssid'),sta_pass:val('sta_pass'),ap_ssid:val('ap_ssid'),ap_pass:val('ap_pass')});refresh(true);}"
"async function resetSettingsConfirm(){if(confirm('Reset all settings and reboot?')){await api('/api/reset','POST',{});}}"
"['profile_name_combo','rotation_dir','rpm','turn_grad','min_angle','max_angle'].forEach(id=>{const el=document.getElementById(id);el.addEventListener('input',updateDirty);});"
"document.getElementById('profile_name_combo').addEventListener('change',onProfileNameChanged);"
"document.getElementById('profile_select').addEventListener('change',onProfileSelectChanged);"
"document.getElementById('rotation_dir').addEventListener('change',updateDirty);"
"refresh(true);setInterval(()=>refresh(false),1000);"
"</script></body></html>";

static int http_read_body(httpd_req_t *req, char *buf, size_t buf_sz) {
    int total = req->content_len;
    if (total <= 0 || total >= (int)buf_sz) return -1;

    int cur = 0;
    while (cur < total) {
        int r = httpd_req_recv(req, buf + cur, total - cur);
        if (r <= 0) return -1;
        cur += r;
    }

    buf[cur] = '\0';
    return cur;
}

static void json_add_profiles(cJSON *root) {
    cJSON *arr = cJSON_CreateArray();

    for (int i = 0; i < g_cfg.profile_count; i++) {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "name", g_cfg.profiles[i].name);
        cJSON_AddNumberToObject(p, "rotation_dir", g_cfg.profiles[i].rotation_dir);
        cJSON_AddNumberToObject(p, "rpm", g_cfg.profiles[i].rpm);
        cJSON_AddNumberToObject(p, "turn_grad", g_cfg.profiles[i].turn_grad);
        cJSON_AddNumberToObject(p, "min_angle", g_cfg.profiles[i].min_angle);
        cJSON_AddNumberToObject(p, "max_angle", g_cfg.profiles[i].max_angle);
        cJSON_AddItemToArray(arr, p);
    }

    cJSON_AddItemToObject(root, "profiles", arr);
}

static esp_err_t root_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, s_index_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "wifi_mode", g_cfg.wifi_mode);
    cJSON_AddStringToObject(root, "sta_ssid", g_cfg.sta_ssid);
    cJSON_AddStringToObject(root, "sta_pass", g_cfg.sta_pass);
    cJSON_AddStringToObject(root, "ap_ssid", g_cfg.ap_ssid);
    cJSON_AddStringToObject(root, "ap_pass", g_cfg.ap_pass);
    cJSON_AddBoolToObject(root, "power_on", g_cfg.power_on);
    cJSON_AddBoolToObject(root, "home_requested", g_home_requested);
    cJSON_AddNumberToObject(root, "rotation_dir", g_cfg.rotation_dir);
    cJSON_AddNumberToObject(root, "rpm", g_cfg.rpm);
    cJSON_AddNumberToObject(root, "turn_grad", g_cfg.turn_grad);
    cJSON_AddNumberToObject(root, "min_angle", g_cfg.min_angle);
    cJSON_AddNumberToObject(root, "max_angle", g_cfg.max_angle);
    cJSON_AddNumberToObject(root, "current_angle", g_current_angle);
    cJSON_AddNumberToObject(root, "table_angle_deg", g_table_angle_deg);
    cJSON_AddNumberToObject(root, "active_profile", g_cfg.active_profile);
    json_add_profiles(root);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    free(out);
    return ret;
}

static esp_err_t start_post(httpd_req_t *req) {
    g_home_requested = false;
    g_cfg.power_on = true;
    cfg_save();
    httpd_resp_sendstr(req, "{}");
    return ESP_OK;
}

static esp_err_t stop_post(httpd_req_t *req) {
    g_home_requested = false;
    g_cfg.power_on = false;
    cfg_save();
    httpd_resp_sendstr(req, "{}");
    return ESP_OK;
}

static esp_err_t home_post(httpd_req_t *req) {
    g_cfg.power_on = false;
    g_home_requested = true;
    cfg_save();
    httpd_resp_sendstr(req, "{}");
    return ESP_OK;
}

static esp_err_t power_post(httpd_req_t *req) {
    g_cfg.power_on = !g_cfg.power_on;
    if (g_cfg.power_on) g_home_requested = false;
    cfg_save();
    httpd_resp_sendstr(req, "{}");
    return ESP_OK;
}

static esp_err_t reset_post(httpd_req_t *req) {
    nvs_flash_erase();
    httpd_resp_sendstr(req, "{}");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}

static esp_err_t config_post(httpd_req_t *req) {
    char body[768];
    if (http_read_body(req, body, sizeof(body)) < 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
    }

    cJSON *j = cJSON_Parse(body);
    if (!j) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");

    cJSON *v;
    if ((v = cJSON_GetObjectItem(j, "wifi_mode")) && cJSON_IsNumber(v)) g_cfg.wifi_mode = (int)v->valuedouble;
    if ((v = cJSON_GetObjectItem(j, "sta_ssid")) && cJSON_IsString(v)) snprintf(g_cfg.sta_ssid, sizeof(g_cfg.sta_ssid), "%s", v->valuestring);
    if ((v = cJSON_GetObjectItem(j, "sta_pass")) && cJSON_IsString(v)) snprintf(g_cfg.sta_pass, sizeof(g_cfg.sta_pass), "%s", v->valuestring);
    if ((v = cJSON_GetObjectItem(j, "ap_ssid")) && cJSON_IsString(v)) snprintf(g_cfg.ap_ssid, sizeof(g_cfg.ap_ssid), "%s", v->valuestring);
    if ((v = cJSON_GetObjectItem(j, "ap_pass")) && cJSON_IsString(v)) snprintf(g_cfg.ap_pass, sizeof(g_cfg.ap_pass), "%s", v->valuestring);
    if ((v = cJSON_GetObjectItem(j, "rotation_dir")) && cJSON_IsNumber(v)) g_cfg.rotation_dir = (int)v->valuedouble;
    if ((v = cJSON_GetObjectItem(j, "rpm")) && cJSON_IsNumber(v)) g_cfg.rpm = (float)v->valuedouble;
    if ((v = cJSON_GetObjectItem(j, "turn_grad")) && cJSON_IsNumber(v)) g_cfg.turn_grad = (float)v->valuedouble;
    if ((v = cJSON_GetObjectItem(j, "min_angle")) && cJSON_IsNumber(v)) g_cfg.min_angle = (float)v->valuedouble;
    if ((v = cJSON_GetObjectItem(j, "max_angle")) && cJSON_IsNumber(v)) g_cfg.max_angle = (float)v->valuedouble;

    cfg_clamp(&g_cfg);
    cfg_save();

    cJSON_Delete(j);
    httpd_resp_sendstr(req, "{}");
    return ESP_OK;
}

static bool profile_name_is_empty(const char *name) {
    if (!name) return true;
    while (*name) {
        if (!isspace((unsigned char)*name)) return false;
        name++;
    }
    return true;
}

static bool profile_name_exists(const char *name, int skip_idx) {
    for (int i = 0; i < g_cfg.profile_count; i++) {
        if (i == skip_idx) continue;
        if (strcmp(g_cfg.profiles[i].name, name) == 0) return true;
    }
    return false;
}

static esp_err_t profiles_post(httpd_req_t *req) {
    char body[768];
    if (http_read_body(req, body, sizeof(body)) < 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
    }

    cJSON *j = cJSON_Parse(body);
    if (!j) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");

    cJSON *op = cJSON_GetObjectItem(j, "op");
    cJSON *idx = cJSON_GetObjectItem(j, "index");
    cJSON *name = cJSON_GetObjectItem(j, "name");

    if (!cJSON_IsString(op)) {
        cJSON_Delete(j);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing op");
    }

    if (strcmp(op->valuestring, "create") == 0) {
        if (g_cfg.profile_count < MAX_PROFILES) {
            profile_t *p = &g_cfg.profiles[g_cfg.profile_count++];
            snprintf(p->name, sizeof(p->name), "%s", cJSON_IsString(name) ? name->valuestring : "");
            p->rotation_dir = g_cfg.rotation_dir;
            p->rpm = g_cfg.rpm;
            p->turn_grad = g_cfg.turn_grad;
            p->min_angle = g_cfg.min_angle;
            p->max_angle = g_cfg.max_angle;
            g_cfg.active_profile = g_cfg.profile_count - 1;
        }
    } else if (strcmp(op->valuestring, "save") == 0) {
        int i = cJSON_IsNumber(idx) ? (int)idx->valuedouble : -1;
        if (i < 0 || i >= g_cfg.profile_count) {
            cJSON_Delete(j);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid profile index");
        }
        if (!cJSON_IsString(name) || profile_name_is_empty(name->valuestring)) {
            cJSON_Delete(j);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "profile name must not be empty");
        }
        if (profile_name_exists(name->valuestring, i)) {
            cJSON_Delete(j);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "profile name must be unique");
        }

        cJSON *rpm = cJSON_GetObjectItem(j, "rpm");
        cJSON *rotation_dir = cJSON_GetObjectItem(j, "rotation_dir");
        cJSON *turn_grad = cJSON_GetObjectItem(j, "turn_grad");
        cJSON *min_angle = cJSON_GetObjectItem(j, "min_angle");
        cJSON *max_angle = cJSON_GetObjectItem(j, "max_angle");
        if (!cJSON_IsNumber(rpm) || !cJSON_IsNumber(rotation_dir) || !cJSON_IsNumber(turn_grad) || !cJSON_IsNumber(min_angle) || !cJSON_IsNumber(max_angle)) {
            cJSON_Delete(j);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing profile params");
        }

        profile_t *p = &g_cfg.profiles[i];
        snprintf(p->name, sizeof(p->name), "%s", name->valuestring);
        p->rotation_dir = (int)rotation_dir->valuedouble;
        p->rpm = (float)rpm->valuedouble;
        p->turn_grad = (float)turn_grad->valuedouble;
        p->min_angle = (float)min_angle->valuedouble;
        p->max_angle = (float)max_angle->valuedouble;
        g_cfg.active_profile = i;
        apply_profile(i);
    } else if (strcmp(op->valuestring, "copy") == 0) {
        int i = cJSON_IsNumber(idx) ? (int)idx->valuedouble : g_cfg.active_profile;
        if (i >= 0 && i < g_cfg.profile_count && g_cfg.profile_count < MAX_PROFILES) {
            g_cfg.profiles[g_cfg.profile_count] = g_cfg.profiles[i];
            if (cJSON_IsString(name)) {
                snprintf(g_cfg.profiles[g_cfg.profile_count].name, sizeof(g_cfg.profiles[g_cfg.profile_count].name), "%s", name->valuestring);
            }
            g_cfg.profile_count++;
        }
    } else if (strcmp(op->valuestring, "delete") == 0) {
        int i = cJSON_IsNumber(idx) ? (int)idx->valuedouble : -1;
        if (i >= 0 && i < g_cfg.profile_count && g_cfg.profile_count > 1) {
            for (int k = i; k < g_cfg.profile_count - 1; k++) {
                g_cfg.profiles[k] = g_cfg.profiles[k + 1];
            }
            g_cfg.profile_count--;
            if (g_cfg.active_profile >= g_cfg.profile_count) g_cfg.active_profile = g_cfg.profile_count - 1;
        }
    } else if (strcmp(op->valuestring, "activate") == 0) {
        int i = cJSON_IsNumber(idx) ? (int)idx->valuedouble : 0;
        apply_profile(i);
    } else {
        cJSON_Delete(j);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown op");
    }

    cfg_clamp(&g_cfg);
    cfg_save();

    cJSON_Delete(j);
    httpd_resp_sendstr(req, "{}");
    return ESP_OK;
}

void web_server_start(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 16;

    ESP_ERROR_CHECK(httpd_start(&s_httpd, &cfg));

    httpd_uri_t u_root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
    httpd_uri_t u_status = {.uri = "/api/status", .method = HTTP_GET, .handler = status_get};
    httpd_uri_t u_power = {.uri = "/api/power", .method = HTTP_POST, .handler = power_post};
    httpd_uri_t u_start = {.uri = "/api/start", .method = HTTP_POST, .handler = start_post};
    httpd_uri_t u_stop = {.uri = "/api/stop", .method = HTTP_POST, .handler = stop_post};
    httpd_uri_t u_home = {.uri = "/api/home", .method = HTTP_POST, .handler = home_post};
    httpd_uri_t u_reset = {.uri = "/api/reset", .method = HTTP_POST, .handler = reset_post};
    httpd_uri_t u_cfg = {.uri = "/api/config", .method = HTTP_POST, .handler = config_post};
    httpd_uri_t u_profiles = {.uri = "/api/profiles", .method = HTTP_POST, .handler = profiles_post};

    httpd_register_uri_handler(s_httpd, &u_root);
    httpd_register_uri_handler(s_httpd, &u_status);
    httpd_register_uri_handler(s_httpd, &u_power);
    httpd_register_uri_handler(s_httpd, &u_start);
    httpd_register_uri_handler(s_httpd, &u_stop);
    httpd_register_uri_handler(s_httpd, &u_home);
    httpd_register_uri_handler(s_httpd, &u_reset);
    httpd_register_uri_handler(s_httpd, &u_cfg);
    httpd_register_uri_handler(s_httpd, &u_profiles);
}
