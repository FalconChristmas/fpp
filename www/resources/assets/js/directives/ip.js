import $ from "jquery";
import InputMask from "jquery.inputmask";

export default {
  twoWay: true,
  bind: function () {
    var self = this;
    var mask = "i[i[i]].i[i[i]].i[i[i]].i[i[i]]";
   
    $(this.el).inputmask(mask);
        
    $(self.el).change(function() {
        var value = $(this).val();
        self.set(value);
    });
  },
 
  unbind: function () {
    var mask = "i[i[i]].i[i[i]].i[i[i]].i[i[i]]";
    $(this.el).unmask(mask); //this.im.unmask(this.im);
  }

}