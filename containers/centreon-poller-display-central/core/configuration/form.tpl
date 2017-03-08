<form {$form.attributes}>
    <table class="formTable table">
        <tr class="list_lvl_1">
            <td class="ListColLvl1_name" colspan="2">
                <h4>{$form.header.title}</h4>
            </td>
        </tr>
        <tr class="list_one">
            <td class="FormRowField">
                <img class="helpTooltip" name="poller_display">{$form.poller_display.label}
            </td>
            <td class="FormRowValue">
                {$form.poller_display.html}
            </td>
        </tr>
    </table>

    {if !$valid}
    <div id="validForm">
        <p>{$form.submitC.html}{$form.submitA.html}</p>
    </div>
    {else}
    <div id="validForm">
        <p>{$form.change.html}</p>
    </div>
    {/if}

{$form.hidden}
</form>
{$helpText}
