<script type="text/javascript" src="./include/common/javascript/tool.js"></script>
<script type="text/javascript" src="./include/common/javascript/resize_td.js"></script>
<script type="text/javascript" src="./include/common/javascript/handlebars-v4.0.5.js"></script>
<script type="text/javascript" src="./include/common/javascript/tooltipster/jquery.tooltipster.min.js"></script>
<script type="text/javascript" src="./modules/centreon-pp-manager/static/js/jquery.fittext.js"></script>
<script type="text/javascript" src="./modules/centreon-pp-manager/static/js/centreon-infinite-scroll.js"></script>
<script type="text/javascript" src="./modules/centreon-pp-manager/static/js/nprogress.js"></script>
<script type="text/javascript" src="./include/common/javascript/jquery/plugins/qtip/jquery-qtip.js"></script>
<script type="text/javascript" src="./modules/centreon-pp-manager/static/js/pluginpack.js"></script>
<script type="text/javascript" src="./include/common/javascript/marked.js"></script>
<link rel="stylesheet" type="text/css" href="./modules/centreon-pp-manager/static/css/style.css" />
<link rel="stylesheet" type="text/css" href="./modules/centreon-pp-manager/static/css/nprogress.css" />
<link rel="stylesheet" type="text/css"  href="./include/common/javascript/tooltipster/tooltipster.css" />
<link rel="stylesheet" type="text/css" href="./Themes/Centreon-2/markdown.css">
<div class="pluginpack-block">
  <h2>{$labels.title_page}</h2>
    {if !$hasLicense}
        {if $isAuth}
            <a href="http://support.centreon.com" id='helpdesk' name='helpdesk' target="_blank" class="btc ppm-orange-button toHideInFullscreen">{t}Online Helpdesk{/t}</a>
        {else}
            <a href="http://www.centreon.com/imp-subscription" id='imp_subscription_button' name='imp_subscription_button' target="_blank" class="btc ppm-orange-button toHideInFullscreen">{t}Subscription{/t}</a>
        {/if}
    {/if}
  <form name='form' method='POST'>
    <table class="ajaxOption table">
      <tr>
        <th><h5>{t}Filters{/t}</h5></th>
      </tr>
      <tr>
        <td><h4>{t}Keyword{/t}</h4></td>
        <td><h4>{t}Category{/t}</h4></td>
        <td><h4>{t}Status{/t}</h4></td>
        <td><h4>{$lastUpdate}</h4></td>
      </tr>
      <tr>
        <td><input type="text" name="name" id="name" size="12"></td>
        <td>
            <select name="category" id="category">
                {foreach from=$categories key=cle item=valeur}
                    {if $valeur == ""}
                        <option value="{$cle}">{$valeur}</option>
                    {else}
                        <option value="{$cle}">{t}{$valeur}{/t}</option>
                    {/if}
                {/foreach}
            </select>
        </td>
        <td>
            <select name="status" id="status">
                 {foreach from=$status_type key=cle item=valeur}
                     <option value="{$cle}">{t}{$valeur}{/t}</option>
                 {/foreach}
            </select>
        </td>
        <td>
            <select name="operator" id="operator">
                 {foreach from=$aOperators key=cle1 item=valeur1}
                     <option value="{$cle1}">{$valeur1}</option>
                 {/foreach}
            </select>
            <input type="text" id="lastUpdate" name="lastUpdate" size="8">
        </td>
        <td><button id='search' name='search' class="btc bt_success">{t}Search{/t}</button></td>
      </tr>
    </table>
  </form>
  <div id="progress-container"></div>
  <h3 id="no-item-message">{t}No plugin packs found.{/t}</h3>
  <div id="pluginpack-container-list">
    <div id="pluginpack-list" class="pluginpack-list"></div>
  </div>
    <div id="uninstalledObjects" class="inactive">

    </div>
    <div id="errorIncheckDependency" class="inactive">
        <p></p>
        <div class="button_group_center">
            <button type="button" class="btc bt_default">{t}Back{/t}</button>
        </div>
    </div>
    <div id="failedActionMessage" class="inactive">
        <p></p>
        <div class="button_group_center">
            <button type="button" class="btc bt_success">{t}OK{/t}</button>
        </div>
    </div>
    <div id="checkRemovePluginPackViewConfirm" class="inactive">
        <p id="checkRemovePluginPackViewConfirm-msg"></p>
    </div>
    <div id="checkDependencyPPViewConfirm" class="inactive">
        <p>{t}In order to install the plugin pack{/t} "<span id="pp-name-in-check-dependency"></span>"</p>
        <div id="plugin-to-install-message">
            <p>{t}the following plugin pack will be installed :{/t} </p>
            <p id="plugin-to-install-message-slugs"></p>
        </div>
        <br>
        <div id="plugin-to-upgrade-message">
            <p>{t}the following plugin pack will be updated :{/t} </p>
            <p id="plugin-to-upgrade-message-slugs"></p>
        </div>
        <div class="button_group_center">
            <button type="button" class="btc bt_success">{t}Apply{/t}</button>
            <button type="button" class="btc bt_default">{t}Cancel{/t}</button>
        </div>
    </div>
    <div id="removePPViewConfirm" class="inactive">
        <p>{t}Do you want to delete this plugin pack ?{/t}</p>
        <div class="button_group_center">
            <button type="button" class="btc bt_danger">{t}Delete{/t}</button>
            <button type="button" class="btc bt_default">{t}Cancel{/t}</button>
        </div>
    </div>
    <div id="updatePPViewConfirm" class="inactive">
        <p>{t}Do you want to update this plugin pack ?{/t}</p>
        <div class="button_group_center">
            <button type="button" class="btc bt_info">{t}Update{/t}</button>
            <button type="button" class="btc bt_default">{t}Cancel{/t}</button>
        </div>
    </div>

    <div id="pluginpack-detail" class="pluginpack-detail-panel inactive">
        <div class="pp-header">
            <div class="pp-header-left">
                <div class="pp-icon" id="pp-logo"></div>
            </div>
            <div id="pp-detail" class="pp-header-right" data-id="" data-name="" data-slug="" data-version="">
                <h2 id="pp-name"></h2>
                <div id="pp-detail-versioning">
                    <p id="pp-version"></p> <span class="badge" id="pp-status"></span>
                </div>
                <div class="detail-action-bar" id="pp-detail-action-bar"></div>
            </div>
        </div>
        <div class="pp-body">
            <p id="pp-description" class="full-page md-preview"></p>
            <div class="pp-details-wrapper">
                <a href="http://www.centreon.com/imp-subscription" id="subscription" name="subscription" class="btc bt_info">{t}Subscription{/t}</a>
            </div>
        </div>

    </div>
</div>