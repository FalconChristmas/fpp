/*==================================================*
 $Id: verifynotify.js,v 1.1 2003/09/18 02:48:36 pat Exp $
 Copyright 2003 Patrick Fitzgerald
 http://www.barelyfitz.com/webdesign/articles/verify-notify/

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *==================================================*/

function verifynotify(field1, field2, result_id, match_html, nomatch_html, button_disable) {
 this.field1 = field1;
 this.field2 = field2;
 this.result_id = result_id;
 this.match_html = match_html;
 this.nomatch_html = nomatch_html;
 this.button_disable = button_disable;

 this.check = function() {

   // Make sure we don't cause an error
   // for browsers that do not support getElementById
   if (!this.result_id) { return false; }
   if (!document.getElementById){ return false; }
   r = document.getElementById(this.result_id);
   if (!r){ return false; }

   if (this.field1.value != "" && this.field1.value == this.field2.value) {
     r.innerHTML = this.match_html;
	 var j = document.getElementById("submit_button");
     j.disabled = false; 
	$('#submit_button').removeClass('disableButtons');
	$('#submit_button').addClass('buttons');
   } else {
     r.innerHTML = this.nomatch_html;
	 //$("#submit_button").attr("disabled", "disabled");
	 var j = document.getElementById("submit_button");
     j.disabled = true; 
	$('#submit_button').removeClass('buttons');
	$('#submit_button').addClass('disableButtons');
   }
 }
}
