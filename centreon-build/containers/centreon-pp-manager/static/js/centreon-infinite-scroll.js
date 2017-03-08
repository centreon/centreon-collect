/**
 * Copyright 2015 Centreon (http://www.centreon.com/)
 * 
 * Centreon is a full-fledged industry-strength solution that meets 
 * the needs in IT infrastructure and application monitoring for 
 * service performance.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0  
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * For more information : contact@centreon.com
 * 
 */

/*global jQuery:false */
/**
 * Infinite scroll into a div
 */
(function($) {
  function CentreonInfiniteScroll(settings, $elem) {
    var self = this;
    this.settings = settings;
    this.$elem = $elem;
    this.loading = true;
    this.hasEvent = true;
    this.lastScroll = 0;
    this.inLoading = false;
    this.$parent = $elem.parent();
    this.noItemMessage = this.settings.noItemMessage;
    this.offset = 0;

    /* Prepare templates */
    this.template = null;
    if (this.settings.template !== "" && this.settings.template !== undefined) {
      this.template = this.settings.template;
    }

    /* Add event to scroll */
    this.$parent.on("scroll", function (e) {
      if (self.hasEvent) {
        if (self.lastScroll < $(this).scrollTop()) {
          var elemHeight = $(self.$elem.children()[1]).height();
          if (self.$parent.scrollTop() + self.$parent.height() > self.$elem.height() - elemHeight) {
            self.loadData();
          }
        }
      }
      self.lastScroll = $(this).scrollTop();
    });

    self.loadData();
  }

  CentreonInfiniteScroll.prototype = {
    loadData: function (ajax) {
      var self = this;
      var ajax = ajax !== undefined ? ajax : true;
      var data = {
        offset: self.offset,
        limit: self.settings.limit
      };
      if ( this.settings.ajaxUrlGetScroll === "" ) {
        return;
      }
      if (this.inLoading && ajax) {
        return;
      }

      if (ajax) {
        NProgress.start();
        this.inLoading = true;
      }

      $.ajax({
        url: this.settings.ajaxUrlGetScroll,
        type: "get",
        dataType: "json",
        data: data,
        success: function (data, statusText, jqXHR) {
          self.offset += self.settings.limit;
          if (data.data == null) {
            self.hasEvent = false;
            self.loading = false;
            self.inLoading = false;
            NProgress.done();
            return;
          }
          $.each(data.data, function (idx, values) {
            var line;
            /* Insert with template */
            if (self.template !== null) {
              line = self.template(values);
              self.$elem.append($(line));
            }
          });
          
          /* Display or hide no item message */
          if (self.noItemMessage !== null) {
            if (self.$elem.children().length > 0) {
                self.noItemMessage.hide();
            } else {
                self.noItemMessage.show();
            }
          }

          /* Continue to load in first call */
          if (self.loading) {
            var elemHeight = $(self.$elem.children()[1]).height();
            if (self.$parent.scrollTop() + self.$parent.height() > self.$elem.height() - elemHeight) {
              self.loadData(false);
            } else {
              NProgress.done();
              self.loading = false;
            }
          }

          /* Send trigger for loaded data */
          if (!self.loading) {
            self.$elem.trigger("loaded");
          }
          if (ajax) {
            NProgress.done();
            self.inLoading = false;
          }
          
          // Resize pp name
          $(".name").fitText(1.2, { minFontSize: '11px', maxFontSize: '16px' });
        }
      });
    },
    prepareData: function () {
      var data = {};
      /* Get filter from form */
      if (this.settings.formFilter !== "") {
        $.each($(this.settings.formFilter).serializeArray(), function (idx, field) {
          if (field.value !== "") {
            if (field.name in data) {
              if (data[field.name] instanceof Array) {
                data[field.name].push(field.value);
              } else {
                tmpValue = data[field.name];
                data[field.name] = [];
                data[field.name].push(tmpValue);
                data[field.name].push(field.value);
              }
            } else {
              data[field.name] = field.value;
            }
          }
        });
      }
      return data;
    }
  };

  $.fn.centreonInfiniteScroll = function (options) {
    var args = Array.prototype.slice.call(arguments, 1);
    var settings = $.extend({}, $.fn.centreonInfiniteScroll.defaults, options);
    var methodReturn = undefined;
    var $set = this.each(function () {
      var $this = $(this);
      var data = $this.data("centreonInfiniteScroll");

      if (!data) {
        $this.data("centreonInfiniteScroll", ( data = new CentreonInfiniteScroll(settings, $this)));
      }

      if (typeof options === "string") {
        methodReturn = data[options].apply(data, args);
      }

      return (methodReturn === undefined) ? $set : methodReturn;
    });
  };

  $.fn.centreonInfiniteScroll.defaults = {
    ajaxUrlGetScroll: "",
    padding: 10,
    limit: 20,
    formFilter: "",
    template: "",
    noItemMessage: null
  };
})(jQuery);
