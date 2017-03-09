$$('td.FormRowField').each(function(el) {        
    var input = el.next().down('input,select,textarea');
    if (typeof(input) != 'undefined') {
        name = input.getAttribute('name');
        el.update('<img class="helpTooltip" name="'+name+'">&nbsp;'+el.innerHTML);
    }
});
