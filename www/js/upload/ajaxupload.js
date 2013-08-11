/**
 * jQuery Real Ajax Uploader 2.7
 * http://www.albanx.com/
 *
 * Copyright 2010-2013, Alban Xhaferllari
 *
 * Date: 12-05-2013
 */

(function($)
{
	//String used in the uploader : STRING: POSITION
	var strings = {
			'Add files':0,
			'Start upload':1,
			'Remove all':2,
			'Close':3,
			'Select Files':4,
			'Preview':5,
			'Remove file':6,
			'Bytes':7,
			'KB':8,
			'MB':9,
			'GB':10,
			'Upload aborted':11,
			'Upload all files':12,
			'Select Files or Drag&Drop Files':13,
			'File uploaded 100%':14,
			'Max files number reached':15,
			'Extension not allowed':16,
			'File size now allowed':17
	};
	
	//translations for STRINGS in array for each language
	var I18N = {
			'it_IT':[
				'Aggiungi file',
				'Inizia caricamento',
				'Rimuvi tutti',
				'Chiudi',
				'Seleziona',
				'Anteprima',
				'Rimuovi file',
				'Bytes',
				'KB',
				'MB',
				'GB',
				'Interroto',
				'Carica tutto',
				'Seleziona o Trascina qui i file',
				'File caricato 100%',
				'Numero massimo di file superato',
				'Estensione file non permessa',
				'Dimensione file non permessa'
			],
			'sq_AL':[
				'Shto file',
				'Fillo karikimin',
				'Hiqi te gjithë',
				'Mbyll',
				'Zgjith filet',
				'Miniaturë',
				'Hiqe file-in',
				'Bytes',
				'KB',
				'MB',
				'GB',
				'Karikimi u ndërpre',
				'Kariko të gjithë',
				'Zgjith ose Zvarrit dokumentat këtu',
				'File u karikua 100%'
			],
			'nl_NL':[
			    'Bestanden toevoegen',
			    'Start uploaden',
			    'Verwijder alles',
			    'Sluiten',
			    'Selecteer bestanden',
			    'Voorbeeld',
			    'Verwijder bestand',
			    'Bytes',
			    'KB',
			    'MB',
			    'GB',
			    'Upload afgebroken',
			    'Upload alle bestanden',
			    'Selecteer bestanden of  Drag&Drop bestanden',
			    'Bestand geüpload 100%'
			   ],
			   'de_DE':[
                'Dateien hinzufügen',
                'Hochladen',
                'Alle entfernen',
                'Schliessen',
                'Dateien wählen',
                'Vorschau',
                'Datei entfernen',
                'Bytes',
                'KB',
                'MB',
                'GB',
                'Upload abgebrochen',
                'Alle hochgeladen',
                'Wählen Sie Dateien oder fügen Sie sie mit Drag & Drop hinzu.',
                'Upload 100%'
          ],
		   'fr_FR':[
               'Ajouter',
               'Envoyer',
               'Tout supprimer',
               'Fermer',
               'Parcourir',
               'Visualiser',
               'Supprimer fichier',
               'Bytes',
               'Ko',
               'Mo',
               'Go',
               'Envoi annulé',
               'Tout envoyer',
               'Parcourir ou Glisser/Déposer',
               'Fichier envoyé 100%']
	};
	
	//current loaded translation
	var AX_I18N = {};
	function load18(lang){
		AX_I18N = I18N[lang];
	}
	
	//the translation function
	function _(s) {
		return AX_I18N ? (AX_I18N[strings[s]] || s) : s;
	}

	// FIXME file class
	//File object, contains information about file, the upload status of file and some callback functions
	var fileObject = function(file, name, size, ext, AU)
	{
		//add proprieties
		this.file 		= file; //real dom file object
		this.currentByte= 0; 	//current uploaded byte
		this.status 	= 0; 	//status -1 error, 0 idle 1 done, 2 upload started
		this.name		= name; //name of file
		this.size		= size; //size of file
		this.xhr		= null; //xmlhttprequest object or form 
		this.info		= null; //info about upload status
		this.ext 		= ext; 	//file extension
		this.pos		= AU.files.length; //position of file in the array needed for file remove
		this.AU 		= AU; 	//AjaxUploader object
		var settings 	= AU.settings;
		
		//create visual part
		var size			= this.sizeFormat();
	    this.li				= $('<li />').appendTo(this.AU.fileList).attr('title', name);//li element container
	    if(settings.bootstrap)
	    {
	    	this.li = $('<a />').appendTo(this.li);
	    }
	    
	    this.prevContainer	= $('<a class="ax-prev-container" />').appendTo( this.li );//preview container
	    this.prevImage		= $('<img class="ax-preview" src="" alt="' + _('Preview') + '" />').appendTo( this.prevContainer ); //preview image
	    this.details		= $('<div class="ax-details" />').appendTo( this.li ); //div containing details of files
	    this.nameContainer	= $('<div class="ax-file-name">'+name+'</div>').appendTo( this.details ); //name container
	    this.sizeContainer	= $('<div class="ax-file-size">'+size+'</div>').appendTo( this.details ); //size container
    	this.progressInfo	= $('<div class="ax-progress" />').appendTo( this.li ); //progress infomation container
	    this.progressBar	= $('<div class="ax-progress-bar" />').appendTo( this.progressInfo ); //animated progress bar
	    this.progressPer	= $('<div class="ax-progress-info">0%</div>').appendTo( this.progressInfo ); //progress percentual
	    this.buttons 		= $('<div class="ax-toolbar" />').appendTo( this.li ); //button container
    	this.uploadButton 	= $('<a title="' + _('Start upload') + '" class="ax-upload ax-button" />').appendTo( this.buttons ).append('<span class="ax-upload-icon ax-icon"></span>');//upload button
    	this.removeButton 	= $('<a title="Remove file" class="ax-remove ax-button" />').appendTo( this.buttons ).append('<span class="ax-clear-icon ax-icon"></span>'); //remove button
    	
    	if(settings.bootstrap)
    	{
    		this.li.addClass('media thumbnail');
    		this.prevContainer.addClass('pull-left');
    		this.prevImage.addClass('img-rounded media-object');
    		
    		this.details.addClass('label label-info').css({ 'border-bottom-left-radius':0});

    		this.progressInfo.addClass('progress progress-striped active').css({'margin-bottom':0});
    		this.progressBar.addClass('bar');
    		
    		this.buttons.addClass('label label-info').css({ 'border-top-left-radius':0, 'border-top-right-radius':0});
    		this.uploadButton.addClass('btn btn-success btn-small').find('span').addClass('icon-play');
    		this.removeButton.addClass('btn btn-danger btn-small').find('span').addClass('icon-minus-sign');
    	}
    	
    	
    	//if ajax upload is not supported then add a form around the file to upload it
    	if(AU.hasHtml4)
    	{
    		var params = this.AU.getParams(name, 0, false);   		
    		//create the upload form
    		var form = $('<form action="'+settings.url+'" method="post" target="ax-main-frame" encType="multipart/form-data" />').hide().appendTo(this.li);
    		form.append(file);	
    		//append to the form eventually the changed name of the uploaded file
        	form.append('<input type="hidden" value="'+name+'" name="ax-file-name" />');//input for re-name of file
			for(var i=0; i<params.length;i++)
			{
				var d = params[i].split('=');
				form.append('<input type="hidden" value="'+d[1]+'" name="'+d[0]+'" />');
			}
			
        	this.xhr = form;
    	}
    	
    	//bind events
    	this.bindEvents();
    	
    	//create the small preview
    	this.doPreview();
    	
		if(settings.hideUploadForm && this.AU.form!==null && this.AU.form!==undefined)
		{
			this.uploadButton.hide();
		}
	};
	
	/**
	 * Creates a human readble size automatically in bytes, mb, kb....
	 * @returns {String}
	 */
	fileObject.prototype.sizeFormat = function()
    {
		var size = this.size;
		
		if (typeof(precision) =='undefined' ) precision = 2;
		
		var suffix = new Array(_('Bytes'), _('KB'), _('MB'), _('GB'));
		var i=0;
		
	    while (size >= 1024 && (i < (suffix.length - 1))) {
	        size /= 1024;
	        i++;
	    }
	    var intVal = Math.round(size);
	    var multp = Math.pow(10, precision);
	    var floor = Math.round((size*multp) % multp);
	    return intVal+'.'+floor+' '+suffix[i];
    };

    /**
     * Bind action events, upload, remove, preview....
     */   
	fileObject.prototype.bindEvents = function(){

    	//bind start upload
		this.uploadButton.bind('click', this, function(e){
			if(e.data.AU.settings.enable)
			{
				if(e.data.status!=2)//start upload
				{
					e.data.startUpload(false);
					$(this).addClass('ax-abort');
				}
				else//if is uploading then stop on reclick
				{
					e.data.stopUpload();
					$(this).removeClass('ax-abort');
				}
						
			}
		});
		   
		//bind remove file
		this.removeButton.bind('click', this, function(e){ 
			if(e.data.AU.settings.enable) e.data.AU.removeFile(e.data.pos);	
		});
		
	    if(this.AU.settings.editFilename)
	    {
	    	//on double click bind the edit file name
	    	this.nameContainer.bind('dblclick', this, function(e){
	    		if(e.data.AU.settings.enable)
	    		{
			    	e.stopPropagation();
			    	var file_name = e.data.name;
			    	var file_ext = e.data.ext;
			    	//get file name without extension
			    	var file_name_no_ext = file_name.replace('.'+file_ext, '');
			    	$(this).html('<input type="text" value="'+file_name_no_ext+'" />.'+file_ext);
	    		}
		    	
		    }).bind('blur focusout', this, function(e){
	    		e.stopPropagation();
	    		var new_fn = $(this).children('input').val();
	    		if( typeof(new_fn) != 'undefined' )
	    		{
	    			var cleanString = new_fn.replace(/[|&;$%@"<>()+,]/g, '');//remove bad filename chars
	    			var final_fn = cleanString+'.'+e.data.ext;
	    			$(this).html(final_fn);
	    			e.data.name = final_fn;
	    			if(!e.data.AU.hasAjaxUpload)//on form upload also rename input hidden input
	    			{
	    				//change the name of hidden input in the uploader form
	    				e.data.xhr.children('input[name="ax-file-name"]').val(final_fn);
	    			}
	    		}
		    });
	    }
	};
	
	/**
	 * function that creates the preview of images
	 */
	fileObject.prototype.doPreview = function(){
		
		//if filereader html5 preview is supported that make the preview on the fly
   		if (this.AU.hasAjaxUpload && this.file.type.match(/image.*/) && (this.ext=='jpg' || this.ext=='gif' || this.ext=='png') && typeof (FileReader) !== "undefined")
	    {
   			var name = this.name;
   			var me = this;
   			//remove the background image of the preview container
   			this.prevContainer.css('background','none');
   			
   			//the image that will contain the preview
   			var img = this.prevImage;
   			
   			//file reader object for reading and loading image
		    var reader = new FileReader();  
		    reader.onload =function(e) {
		    	//set the image cursort to pointer for indicating
		    	img.css('cursor','pointer').attr('src', e.target.result).click(function(){
		    		
		    		//create a image loader for getting image size
		   			var imgloader = new Image();
		   			imgloader.onload = function()
		   			{
		   				//resize image to fit the user window size
		   			    var ratio = Math.min( $(window).width() / this.width, ($(window).height()-100) / this.height);
		   			    var newWidth = (ratio<1)?this.width * ratio:this.width;
		   			    var newHeight = (ratio<1)?this.height * ratio:this.height;

		   			    var axtop = ($(window).scrollTop()-20+($(window).height()-newHeight)/2);
		   			    var axleft= ($(window).width()-newWidth)/2;
		   			    
		   			    //set preview box position and dimension accordin to screen
		   			    var axbox = $('#ax-box').css({ top:  axtop, height:newHeight, width:newWidth, left: axleft});
		   			    
		   			    //set the preview image
		   			    axbox.children('img').attr({ width: newWidth, height:newHeight, src:e.target.result });
		   			    
		   			    //set the name of the file
		   			    $('#ax-box-fn').find('span').html(name + ' ('+me.sizeFormat()+')');
		   			    
		   			    //then show in the preview
		   			    axbox.fadeIn(500);			
		   			    
		   			    //expand blocking overlay
			    		$('#ax-box-shadow').css('height', $(document).height()).show();
		   			};
		   			//load the image
		   			imgloader.src = e.target.result;
		   			
					$('#ax-box-shadow').css('z-index', 10000);
					$('#ax-box').css('z-index', 10001);
		    	});
		    };  
		    
		    //read file from fs
		    reader.readAsDataURL(this.file); 
	    }
	    else
	    {
	    	//if not supported or is not image file that load the icon of file type
	    	this.prevContainer.addClass('ax-filetype-'+this.ext).children('img:first').remove();
	    }
	};
	
	
	/**
	 * Start upload method
	 * @up_all if true the upload will run for all files
	 */
	fileObject.prototype.startUpload = function(up_all)
	{
		this.AU.upload_all = up_all;
		//check if the before upload returns true, from user validation event
		var valid = this.AU.settings.beforeUpload.call(this, this.name, this.file);
		if(valid)
		{
			this.progressBar.css('width','0%');
			this.progressPer.html('0%');
			this.uploadButton.addClass('ax-abort');
			this.status = 2;//uploading status
			if(this.AU.hasAjaxUpload)//html5 upload
			{
				this.uploadAjax();
			}
			else if(this.AU.hasFlash) //flash upload
			{
				//FIXME has some bug on parallel uploads flash, so we disable the possibility to upload other files meanwhile
				if(!this.AU.uploading)
				{
					this.AU.uploading = true;
					this.AU.flashObj.uploadFile(this.pos);
				}
			}
			else //standard html4 upload
			{
				this.uploadStandard(up_all);
			}
		}
	};
	
	/**
	 * Main upload ajax html5 method, uses xmlhttprequest object for uploading file
	 * Runs in recrusive mode for uploading files by chunk
	 */
	fileObject.prototype.uploadAjax = function(){

		var settings 	= this.AU.settings;
		var file		= this.file;
    	var currentByte	= this.currentByte;
    	var name		= this.name;
    	var size		= this.size;
    	var chunkSize	= settings.chunkSize;	//chunk size
		var endByte		= chunkSize + currentByte;
		var isLast		= (size - endByte <= 0);
    	var chunk		= file;
    	var chunkNum	= endByte / chunkSize;
    	this.xhr 		= new XMLHttpRequest();//prepare xhr for upload
    	
    	if(currentByte == 0)	this.AU.slots++;
    	
    	if(chunkSize == 0)//no divide
    	{
    		chunk	= file;
    		isLast	= true;
    	}
    	else if(file.mozSlice) // moz slice
    	{
    		chunk	= file.mozSlice(currentByte, endByte);
    	}
    	else if(file.webkitSlice) //webkit slice
    	{
    		chunk	= file.webkitSlice(currentByte, endByte);
    	}
    	else if(file.slice) // old slice, there are two version of the same
    	{
    		chunk	= file.slice(currentByte, endByte);
    	}
    	else//no slice
    	{
    		chunk	= file;
    		isLast	= true;
    	}
    	
    	var me = this;
    	//abort event, (nothing to do for the moment)
    	this.xhr.upload.addEventListener('abort', function(e){
    		me.AU.slots--;
    	}, false); 
    	
    	//progress function, with ajax upload progress can be monitored
    	this.xhr.upload.addEventListener('progress', function(e)
		{
			if (e.lengthComputable) 
			{
				var perc = Math.round((e.loaded + chunkNum * chunkSize - chunkSize) * 100 / size);
				me.onProgress(perc);
			}  
		}, false); 
    	    	
    	this.xhr.upload.addEventListener('error', function(e){
    		me.onError(this.responseText);
    	}, false);  
    	
    	this.xhr.onreadystatechange=function()
		{
			if(this.readyState == 4 && this.status == 200)
			{
				try
				{
					var ret	= JSON.parse( this.responseText );
					//get the name returned by the server (it renames eventually)
					if(currentByte == 0)
					{
						me.name	= ret.name;
						me.nameContainer.html(ret.name);
					}
					if(parseInt(ret.status) == -1)
					{
						throw ret.info;
					}
					else if(isLast)
					{
						me.AU.slots--;//free the slot
						//exec finish event of the file
						me.onFinish(ret.name, ret.size, ret.status, ret.info);
					}
					else
					{
						me.currentByte = endByte;
						me.uploadAjax();
					}
				}
				catch(err)
				{
					me.AU.slots--;
					me.onError(err);
				}
			}
		};

		var user_form_data = window.FormData !== undefined;
		
		//if formData is supported we use that, better in general FF4+, Chrome, Safari
		var isfirefox5 =  (navigator.userAgent).match(/Firefox\/(\d+)?/);
		if(isfirefox5!==null)
		{
			var fire_ver = isfirefox5!==null && isfirefox5[1]!==undefined && !isNaN(isfirefox5[1]) ? parseFloat(isfirefox5[1]) : 7;
			if(fire_ver<=6) user_form_data = false;
		}
		
		//same for some version of opera
		var is_opera =  (navigator.userAgent).match(/Opera\/(\d+)?/);
		if(is_opera!==null)
		{
			var ver = (navigator.userAgent).match(/Version\/(\d+)?/);
			var opera_ver = ver[1]!==undefined && !isNaN(ver[1]) ? parseFloat(ver[1]) : 0;
			if(opera_ver<12.10) user_form_data = false;
		}
		
		var params = this.AU.getParams(name, size, !user_form_data);
		params.push('ax-start-byte='+ currentByte);
		params.push('ax-last-chunk='+ isLast);

		if(user_form_data) //firefox 5 formdata does not work correctly
		{
			var data = new FormData();
			data.append('ax_file_input', chunk);
			for(var i=0; i<params.length; i++)
			{
				var d = params[i].split('=');
				data.append(d[0], d[1] );
			}
			this.xhr.open('POST', settings.url, settings.async);
			this.xhr.send(data);
		}
		else//else we use a old trick upload with php::/input ajax, FF3.6+, Chrome, Safari
		{
			var c =  settings.url.indexOf('?')==-1 ?'?':'&';			
			this.xhr.open('POST', settings.url+c+params.join('&'), settings.async);
			this.xhr.setRequestHeader('Cache-Control', 'no-cache');
			this.xhr.setRequestHeader('X-Requested-With', 'XMLHttpRequest');//header
			this.xhr.setRequestHeader('Content-Type', 'application/octet-stream');//generic stream header
			this.xhr.send(chunk);//send peice of file
		}
	};

	
	/**
	 * starndard upload function for normal browsers
	 * @param up_all if set to true after finish upload, uploads the next file
	 */
    fileObject.prototype.uploadStandard = function(up_all)
    {
		//just fake upload in standard forms upload
		this.progressBar.css('width','50%');
		this.progressPer.html('50%');
		$('#ax-main-frame').unbind('load').bind('load', this, function(e){
			
			//get iframe content
			var frameDoc = null;
			if ( this.contentDocument ) 
			{ // FF
				frameDoc = this.contentDocument;
			}
			else if ( this.contentWindow ) 
			{ // IE
				frameDoc = this.contentWindow.document;
			}
    		
			//hope is a json, from which we get file information
			try{
				var ret	= $.parseJSON(frameDoc.body.innerHTML);
	    		//set progress to 100%
	    		e.data.onProgress(100);
	    		
	    		//get file info
	    		e.data.onFinish(ret.name, ret.size, ret.status, ret.info);
			}catch(err){
				e.data.onError(frameDoc.body.innerHTML);
			}
   		
    		//if upload all flag is set then try to upload the next file
    		if(up_all!==undefined && e.data.AU.files[e.data.pos+1]!==undefined)
    		{
    			e.data.AU.files[e.data.pos+1].startUpload(up_all);
    		}
		});
		
		//submit the form of the standard upload
		this.xhr.submit();
    };
    
    
    /**
     * Stop upload function. it reset visual information and if the upload is xhr it calls the abort
     * method, or if upload is, iframe standard based it stops the iframe request
     */
	fileObject.prototype.stopUpload = function(){
		if(this.AU.hasAjaxUpload)
		{
			if(this.xhr!==null)//if is current uploading 
			{
				this.xhr.abort();	//abort xmlhttprequest request
				this.xhr 	= null;//remove the xhr / form object
			}
		}
		else if(this.AU.hasFlash)
		{
			this.AU.flashObj.stopUpload(this.pos);//call flash stop upload method
		}
		else
		{
			var iframe	= document.getElementById('ax-main-frame');
			//stop iframe from uploading
			try{
				iframe.contentWindow.document.execCommand('Stop');
			}
			catch(ex){
				iframe.contentWindow.stop();
			}
		}
		
		this.uploadButton.removeClass('ax-abort');//show upload button
		this.currentByte = 0; //reset current byte
		this.status = 0;//set status to idle
		this.progressBar.css('width', 0);// reset progress bar
		this.progressPer.html( _('Upload aborted') );//print abort info
	};
	

	/**
	 * Runs on error
	 * @param err error return from the server
	 */
	fileObject.prototype.onError = function(err)
	{
    	this.currentByte = 0;
    	this.status	= -1;
    	this.info	= err;
    	this.progressPer.html(err);
    	this.progressBar.css('width','0%');
    	this.uploadButton.removeClass('ax-abort');

    	//trigger the error event
		this.AU.settings.error.call(this, err, this.name);
		
		if(this.AU.settings.removeOnError)
    	{
    		this.AU.removeFile(this.pos);
    	}
	};
	
	fileObject.prototype.onFinish = function(name, size, status, info)
	{
    	this.name	= name;
    	this.status	= parseInt(status);
    	this.info	= info;


    	//in html5 file and flash api we read size from browser, in standard upload we get it from the server
    	if(!this.AU.hasAjaxUpload && !this.AU.hasFlash)
    	{
    		this.size	= size;
    		var size	= this.sizeFormat();
    		this.sizeContainer.html(size);
    	}
    	this.currentByte = 0;
    	this.nameContainer.html(name);//get new name of file from server
    	this.li.attr('title', name);
    	this.onProgress(100);
    	this.uploadButton.removeClass('ax-abort');//remove abort button
    	this.progressBar.width(0);
    	this.progressPer.html(_('File uploaded 100%')); 
    	this.AU.settings.success.call(this, name);//call success method
    	
    	//if all files had been uploaded then exec finish event
    	var runFinish = true;
    	for(var i = 0; i<this.AU.files.length; i++)
    	{
    		//so if we have any file still at idle do not run finish event
    		if(this.AU.files[i].status!=1 && this.AU.files[i].status!=-1) runFinish = false;
    	}
    	
    	if(runFinish && !this.AU.finis_has_run)
    	{
    		this.AU.finish();
    		this.AU.finis_has_run = true;
    	}
    	
    	if(this.AU.settings.removeOnSuccess)
    	{
    		this.AU.removeFile(this.pos);
    	}
	};
	
	/**
	 * Function that is trigger on the progress event of the upload
	 * updates progress bar and percentual
	 */
	fileObject.prototype.onProgress = function(p){
		this.progressBar.css('width',p+'%');
		this.progressPer.html(p+'%');
	};
	
	
	
	/**
	 * Ajax Uploader class FIXME first class
	 */
	var AjaxUploader = function($this, settings)
	{
		//Test if support pure ajax upload and create browse file input
		var axtest 			= document.createElement('input');
		axtest.type 		= 'file';
		this.hasAjaxUpload 	= ('multiple' in axtest &&  typeof File != "undefined" &&  typeof (new XMLHttpRequest()).upload != "undefined" );
		this.hasFlash 		= false;
		axtest 				= null; //avoid memory leak IE
		
		//this.hasAjaxUpload = false;

		//safari<5.1.7 is bugged in file api so we force using flash upload
		var is_bugged_safari =  /Safari/.test(navigator.userAgent) && /Apple Computer/.test(navigator.vendor) &&  /Version\/5\./.test(navigator.userAgent) && /Win/.test(navigator.platform);
		if(is_bugged_safari) this.hasAjaxUpload=false;
		
		//if does not support html5 upload, test if supports flash upload
		if(!this.hasAjaxUpload)
		{
			try 
			{
				var fo = new ActiveXObject('ShockwaveFlash.ShockwaveFlash');
				if(fo) this.hasFlash = true;
			}catch(e){
				if(navigator.mimeTypes ["application/x-shockwave-flash"] != undefined) this.hasFlash = true;
			}
		}
		
		//at the end test if supports only html4 standard upload
		this.hasHtml4 = (!this.hasFlash && !this.hasAjaxUpload);
		
		this.$this 		= $this; 
		this.files 		= [];//array with the fileObjects
		this.slots 		= 0;//trace slots (parallel uploads)
		this.settings 	= settings;//store settings
		this.fieldSet  	= $('<fieldset />').append('<legend class="ax-legend">' + _('Select Files') + '</legend>').appendTo($this);
		this.form 		= null; //if form integration
		this.form_submit_event = null;
		this.finis_has_run = false;
		this.flashObj	= null;
		this.upload_all	= false;
		this.uploading	= false;
				
		//get bytes in real format
		this.max_size 	= settings.maxFileSize;
		var mult 		= settings.maxFileSize.slice(-1);
		if(isNaN(mult))
		{
			this.max_size = this.max_size.replace(mult, '');//remove the last char
			switch (mult)//1024 or 1000??
			{
				case 'T': this.max_size = this.max_size*1024;
				case 'G': this.max_size = this.max_size*1024;
				case 'M': this.max_size = this.max_size*1024;
				case 'K': this.max_size = this.max_size*1024;
			}
		}
		
		var bs_browse = 'ax-browse-c ax-button';
		var bs_upload = 'ax-upload-all ax-button';
		var bs_remove = 'ax-clear ax-button';
		
		var bs_b_icon	= 'ax-plus-icon ax-icon';
		var bs_u_icon	= 'ax-upload-icon ax-icon';
		var bs_r_icon	= 'ax-clear-icon ax-icon';
		
		if(settings.bootstrap)
		{
			bs_browse += ' btn btn-primary';
			bs_upload += ' btn btn-success';
			bs_remove += ' btn btn-danger';
			
			bs_b_icon += ' icon-plus-sign';
			bs_u_icon += ' icon-play';
			bs_r_icon += ' icon-remove-sign';
		}
		
		this.browse_c = $('<a class="'+bs_browse+'" title="' + _('Add files') + '" />').append('<span class="'+bs_b_icon+'"></span> <span class="ax-text">' + _('Add files') + '</span>').appendTo(this.fieldSet);
		
		//Browser control
		this.browseFiles = $('<input type="file" class="ax-browse" name="ax_file_input" />').attr('multiple', this.hasAjaxUpload).appendTo(this.browse_c);
		
		//experimental feature, works only on google chrome, has some perfomance issue, upload directory FIXME
		if(settings.uploadDir)
		{
			this.browseFiles.attr({'directory':'directory', 'webkitdirectory':'webkitdirectory', 'mozdirectory':'mozdirectory'});
		}
		
		//Browse container for the browse control
		if(this.hasFlash)
		{
			//remove the normal html browse in this case to add the flash one
			this.browse_c.children('.ax-browse').remove();
			
			//give to the flash element an id
			var flash_id = $this.attr('id')+'_flash';
			
			//standard cross-browser flash html code
			var flash_html = 	'<!--[if !IE]> -->'+
								'<object style="position:absolute;width:150px;height:100px;left:0px;top:0px;z-index:1000;" id="'+flash_id+'" type="application/x-shockwave-flash" data="'+settings.flash+'" width="150" height="100">'+
								'<!-- <![endif]-->'+
								'<!--[if IE]>'+
								'<object style="position:absolute;width:150px;height:100px;left:0px;top:0px;z-index:1000;" id="'+flash_id+'" classid="clsid:D27CDB6E-AE6D-11cf-96B8-444553540000"  codebase="http://download.macromedia.com/pub/shockwave/cabs/flash/swflash.cab#version=6,0,0,0" width="150" height="100">'+
								'<param name="movie" value="'+settings.flash+'" />'+
								'<!--><!--dgx-->'+
								'<param name="flashvars" value="instance_id='+$this.attr('id')+'">'+
								'<param name="allowScriptAccess" value="always" />'+
								'<param value="transparent" name="wmode">'+
								'</object>'+
								'<!-- <![endif]-->';
			
			this.browse_c.append('<div style="position:absolute;overflow:hidden;width:150px;height:100px;left:0px;top:0px;z-index:0;">'+flash_html+'</div>');

			//keep a reference to the flash object for calling actionscript methods from javascript
			this.flashObj = document.getElementById(flash_id);
		}
				
		//Upload all button
	    this.uploadFiles = $('<a class="'+bs_upload+'" title="' + _('Upload all files') + '" />').append('<span class="'+bs_u_icon+'"></span> <span>' + _('Start upload') + '</span>').appendTo(this.fieldSet);
	    
	    //remove files button
	    this.removeFiles = $('<a class="'+bs_remove+'" title="' + _('Remove all') + '" />').append('<span class="'+bs_r_icon+'"></span> <span>' + _('Remove all') + '</span>').appendTo(this.fieldSet);
	    	    
	    //file list container
	    this.fileList	= $('<ul class="ax-file-list" />').appendTo(this.fieldSet);
	    if(settings.bootstrap)
	    {
	    	this.fileList.addClass('media-list');
	    }
	    

	    //run the init call back
		settings.onInit.call(this, this);
	    
		//bind click mouse event and other events
	    this.bindEvents($this);
	};

	
	/**
	 * Bind events to buttons, drop area
	 * @param $this the dom jquery object
	 */
	AjaxUploader.prototype.bindEvents = function($this)
	{
	    //Add browse event
		var settings = this.settings;
		
	    this.browseFiles.bind('change', this, function(e){
	    	//do not open on flash support, flash has his own browse file
	    	if(e.data.settings.enable && !e.data.hasFlash)
	    	{
				//if is supported ajaxupload then we have an array of files, if not we have a simple input element with one file
				var files = (e.data.hasAjaxUpload) ? this.files : new Array(this);
		    	e.data.addFiles(files);
		    	
		    	if(!e.data.hasAjaxUpload)
		    	{
		    		//clone element for next file select
		    		$(this).clone(true).val('').appendTo(e.data.browse_c);
		    	}
		    	
		    	//chrome fix change event
		    	this.value = '';
	    	}
		});
	    
	    
	    //upload files
	    this.uploadFiles.bind('click', this, function(e){
	    	if(e.data.settings.enable) e.data.uploadAll();
	    	return false;
	    });
	    
	    //remove all files from list
	    this.removeFiles.bind('click', this, function(e){
	    	if(e.data.settings.enable) e.data.clearQueue();
	    	return false;
	    });
	    
	    //external form integration
		//If used with form combination, the bind upload on form submit
		if($(settings.form).length>0)
		{
			this.form = $(settings.form);
		}
		else if(settings.form=='parent')
		{
			this.form = $this.parents('form:first');
		}   
		
		if(this.form !==null && this.form!==undefined)
		{
			//hide upload buttons on form
			if(settings.hideUploadForm){
				this.uploadFiles.hide();
			}
			
			//if form has any binded any submit event by the user store it and execute at the end
			var events = this.form.data("events");
			if(events!==null && events!==undefined)
			{
				if(events.submit!==null && events.submit!==undefined)
				{
					this.form_submit_event = events.submit;
				}
			}
	        
			//now unbind form events
			this.form.unbind('submit');
			
			//before submit form first upload the files
			this.form.bind('submit.ax', this, function(e){
				if(e.data.files.length>0)
				{
					e.data.uploadAll();
					return false;
				}
			});
		}
		
	    //create drop area if is setted and if is supported
	    if(this.hasAjaxUpload)
	    {
	    	var dropArea = (settings.dropArea=='self')? $this[0]: $(settings.dropArea)[0];
	    	var me = this;
	    	
	    	//change the text to drag&drop
	    	if(settings.dropArea == 'self')
	    	{
	    		this.fieldSet.find('.ax-legend').html(_('Select Files or Drag&Drop Files'));
	    	}
	    	
		    //Prevent default and stop propagation on dragenter
	    	dropArea.addEventListener('dragenter',function(e){
	    		e.stopPropagation();  
	    		e.preventDefault(); 
	    	},false);
	    	
	    	//on drag over change background color
	    	dropArea.addEventListener('dragover', function(e){    		
	    		e.stopPropagation();  
	    		e.preventDefault(); 
	    		if(me.settings.enable)
	    		{
	    			if(settings.dropClass) 
	    				$(this).addClass(settings.dropClass);
	    			else
	    				this.style.backgroundColor=settings.dropColor; 
	    			
	    		}
	    	},false);
	    	
	    	//on drag leave reset background color
	    	dropArea.addEventListener('dragleave', function(e){
	    		e.stopPropagation();  
	    		e.preventDefault(); 
	    		if(me.settings.enable)
	    		{
	    			if(settings.dropClass) 
	    				$(this).removeClass(settings.dropClass);
	    			else
	    				this.style.backgroundColor = '';   
	    		}
	    	},false);
	    	
	    	//on drop add files to list
	    	
	    	dropArea.addEventListener('drop', function(e)
		    {
	    		if(me.settings.enable)
	    		{
			    	e.stopPropagation();  
			    	e.preventDefault();
	
			    	//add files
			    	me.addFiles(e.dataTransfer.files);
	
			    	//reset background color
					this.style.backgroundColor = '';
					
					//if autostart is enabled then start upload
			    	if(settings.autoStart)
			    	{
			    		me.uploadAll();
			    	}
	    		}
			},false);	
	    	
	    	//bind the ESC button to close the preview of image, unbind to avoid multiple call when have multiple instance of the uploader
    		$(document).unbind('.ax').bind('keyup.ax',function(e){
    			if (e.keyCode == 27) {
    				$('#ax-box-shadow, #ax-box').fadeOut(500);
    			}  
    		});
	    }
	    
	    //load enable option
	    this.enable(this.settings.enable);
	};


	/**
	 * Event that runs when all files had finish uploaded
	 */
	AjaxUploader.prototype.finish = function()
	{
		//collect file names in a array
		var fileNames 	= []; //file names
		
		for(var i = 0; i < this.files.length; i++)
		{
			fileNames.push(this.files[i].name);
		}
		
		//to the finish event return the file names and files object
		this.settings.finish.call(this, fileNames, this.files);

		this.settings.beforeSubmit.call(this, fileNames, this.files, function(){
			//if there is a form integrated then submit the form and append files informations
			if(this.form!==null && this.form!==undefined)
			{
				//add to the form the file paths
				var basepath = (typeof(this.settings.remotePath)=='function')?this.settings.remotePath():this.settings.remotePath;
				
				for(var i=0;i<fileNames.length;i++)
				{
					var filepath = basepath+fileNames[i];
					this.form.append('<input name="ax-uploaded-files[]" type="hidden" value="'+filepath+'" />');
				}
				
				this.form.unbind('submit.ax');//remove ajax uploader event
				
				//bind his original submit event
				if(this.form_submit_event!==null && this.form_submit_event!==undefined)
				{
					this.form.bind('submit', this.form_submit_event);
				}
				
				var has_submit_button = this.form.find('[type="submit"]');//strange if form has submit button cannot call .submit()

				if(has_submit_button.length>0)
					has_submit_button.trigger('click');//trigger click on the submit button of the form
				else 
					this.form.submit();//submit the form normally now
			}
		});
	};
	
	/**
	 * Function that add selected files to list
	 */
	AjaxUploader.prototype.addFiles = function(files)
	{
		//add selected files to the queue
		for (var i = 0; i < files.length; i++) 
		{
			var ext, name, size;
			
			//get file name and file extenstion
			if(this.hasAjaxUpload || this.hasFlash)
			{
				name	= files[i].name;
				size	= files[i].size;
			}
			else
			{
				name	= files[i].value.replace(/^.*\\/, '');
				size	= 0;
			}

			//normalizze extension
			ext	= name.split('.').pop().toLowerCase();

			//control if extension is allowed to be uploaded 
			//if we have reach the max number of files allowed
			//if file size is allowed
			var err = this.checkFile(name, size);
			if(err == '')
			{
				//create the file object
				var fileObj = new fileObject(files[i], name, size, ext, this );
				//put in queue
				this.files.push( fileObj );
			}
			else
			{
				this.settings.error.call(this, err, name);
			}
		}
		
		//exec after select call back
		this.settings.onSelect.call(this, this.files);

		//if autostart is enabled then start upload
    	if(this.settings.autoStart)
    	{
    		this.uploadAll();
    	}
	};

	AjaxUploader.prototype.checkFile = function(name, size)
	{
		var ext	= name.split('.').pop().toLowerCase();
		
		var max_num_f 	= !!(this.files.length < this.settings.maxFiles);
		var allow_ext 	= !!($.inArray(ext, this.settings.allowExt)>=0 || this.settings.allowExt.length==0);
		var max_size 	= !!(size<=this.max_size);
		
		var error = '';
		if(!max_num_f)	error=+_('Max files number reached')+':' + max_num_f +"\n";
		if(!allow_ext)	error=+_('Extension not allowed')+':' + ext +"\n";
		if(!max_size)	error=+_('File size now allowed')+':' + size + "\n";
		
		return error;
	};
	
	/**
	 * method for uploading all files in one click
	 */
	AjaxUploader.prototype.uploadAll = function()
	{
		var valid = this.settings.beforeUploadAll.call(this, this.files);//validate with beforeuplaodAll first
		if(valid!==false)
		{
			var uploadAllOnce = false;
			var start_from = false;
			for(var i = 0;i < this.files.length; i++)
			{
				if(this.files[i].status == 0)
				{
					uploadAllOnce = true;
					if(!start_from) start_from = this.files[i];
				}
			}
			
			//if all files had been upload then stop uploadAll button
			if(!uploadAllOnce)
			{
				return;
			}
			
			//Ajax can make parallel uploads
			if(this.hasAjaxUpload)
			{
				var ax = this;
		    	setTimeout(function(){
		    		var stop = true;//stop timer
		    		for(var i = 0;i < ax.files.length; i++)//control every 300 ms all files
		    		{
		    			if(ax.files[i].status == 0)//there are files in the 0 status (idle) the try to upload
		    			{
		    				stop = false;//do not stop timer check again
		    				if(ax.slots <= ax.settings.maxConnections)//upload if there are free slots
		    				{
		    					ax.files[i].startUpload(false);
		    				}
		    			}
		    		}
		    		//if there are still files then try to upload them, if there are free slots
		    		if(!stop)	ax.uploadAll();
		    	}, 300);	
			}
			else //for normal standard uploads, flash
			{
				if(start_from)
				{
					start_from.startUpload(true);
				}
			}
		}
	};
	
	/**
	 * Function that remove all files from list, clearing queue, stop running uploads
	 */
	AjaxUploader.prototype.clearQueue = function()
    {
    	while(this.files.length>0){
    		this.removeFile(0);
    	}
    };
    
    AjaxUploader.prototype.getParams = function(file_name, size, encode)
    {
		var settings = this.settings;
		var getpath	= (typeof(settings.remotePath)=='function')?settings.remotePath():settings.remotePath;
		var params	= [];
		
		//file data
		params.push('ax-file-path=' + (encode ? encodeURIComponent(getpath): getpath) );
		params.push('ax-allow-ext=' + (encode ? encodeURIComponent( settings.allowExt.join('|')) : settings.allowExt.join('|')) );
		params.push('ax-file-name=' + (encode ? encodeURIComponent(file_name) : file_name) );
		params.push('ax-max-file-size=' + settings.maxFileSize);
		params.push('ax-file-size=' + size);
		
		//thumb data
		params.push('ax-thumbPostfix=' + (encode ? encodeURIComponent(settings.thumbPostfix) : settings.thumbPostfix) );
		params.push('ax-thumbPath=' + (encode ? encodeURIComponent(settings.thumbPath) : settings.thumbPath) );
		params.push('ax-thumbFormat=' + (encode ? encodeURIComponent(settings.thumbFormat) : settings.thumbFormat) );
		params.push('ax-thumbHeight=' + settings.thumbHeight);
		params.push('ax-thumbWidth=' + settings.thumbWidth);
		

		
		var otherdata	= (typeof(settings.data)=='function')?settings.data():settings.data;
		if(typeof(otherdata)=='object')
		{
			for(var i in otherdata)
			{
				params.push( i + '=' + (encode ? encodeURIComponent(otherdata[i]) : otherdata[i]) );
			}
		}
		else if(typeof(otherdata)=='string' && otherdata!='')
		{
			params.push(otherdata);
		}
		
		return params;
    };

	
	/**
	 * Remove a file from the queue
	 * @param pos position of file to remove from the queue
	 */
	AjaxUploader.prototype.removeFile = function(pos)
	{
		var fileobj = this.files[pos];//get the file to remove
		fileobj.stopUpload();//stop upload if the files is being upload
		fileobj.li.remove();//remove the visual LI
		fileobj.file = null;//remove file dom object
		this.files.splice(pos, 1);//remove the file from the array (same is done in flash internal list)
		
		//remove the file from flash list
		if(this.hasFlash)
		{
			this.flashObj.removeFile(pos);
		}
		//re-calculate file positions
		for(var i=0; i<this.files.length; i++)
		{
			this.files[i].pos = i;
		}
	};
	
	/**
	 * Method to set/get options live
	 * @param opt the option to change or get
	 * @param val if is setted then change option to this val, if it is not given than get option value
	 * @returns option value or null
	 */
	AjaxUploader.prototype.options = function(opt, val){
		if(val!==undefined && val!==null)
		{
			this.settings[opt] = val;
			if(opt == 'enable')
			{
				this.enable(val);
			}
		}
		else
		{
			return this.settings[opt];
		}
	};
	
	AjaxUploader.prototype.enable = function(bool){
		this.settings.enable= bool;
		if(bool)
		{
			this.$this.removeClass('ax-disabled').find('input').attr('disabled',false);
		}
		else
		{
			this.$this.addClass('ax-disabled').find('input').attr('disabled',true);
		}
	};
	
    var globalSettings = 
    {
    	remotePath : 	'uploads/',					//remote upload path, can be set also in the php upload script
    	url:			'upload.php',				//php/asp/jsp upload script
    	flash:			'uploader.swf',				//flash uploader url for not html5 browsers
    	data:			'',							//other user data to send in GET to the php script
    	async:			true,						//set asyncron upload or not
    	maxFiles:		9999,						//max number of files can be selected
    	allowExt:		[],							//array of allowed upload extesion, can be set also in php script
    	success:		function(fn){ },				//function that triggers every time a file is uploaded
    	finish:			function(file_names, file_obj){ },			//function that triggers when all files are uploaded
    	error:			function(err, fn){ },		//function that triggers if an error occuors during upload,
    	enable:			true,						//start plugin enable or disabled
    	chunkSize:		1048576,					//default 1Mb,	//if supported send file to server by chunks, not at once
    	maxConnections:	3,							//max parallel connection on multiupload recomended 3, firefox support 6, only for browsers that support file api
    	dropColor:		'red',						//back color of drag & drop area, hex or rgb
    	dropClass:		'ax-drop',					//class to add to the drop area when dropping files
    	dropArea:		'self',						//set the id or element of area where to drop files. default self
    	autoStart:		false,						//if true upload will start immediately after drop of files or select of files
    	thumbHeight:	0,							//max thumbnial height if set generate thumbnial of images on server side
    	thumbWidth:		0,							//max thumbnial width if set generate thumbnial of images on server side
    	thumbPostfix:	'_thumb',					//set the post fix of generated thumbs, default filename_thumb.ext,
    	thumbPath:		'',							//set the path where thumbs should be saved, if empty path setted as remotePath
    	thumbFormat:	'',							//default same as image, set thumb output format, jpg, png, gif
    	maxFileSize:	'10M',						//max file size of single file,
    	form:			null,						//integration with some form, set the form selector or object, and upload will start on form submit
    	hideUploadForm:	true,						//hide upload button on form integration, upload starts on form submit
    	beforeSubmit: 	function(file_names, file_obj, formsubmitcall){
    		formsubmitcall.call(this);
    	},				//event that runs before submiting a form
    	editFilename:	false,						//if true allow edit file names before upload, by dblclick
		beforeUpload:	function(filename, file){return true;}, //this function runs before upload start for each file, if return false the upload does not start
    	beforeUploadAll:function(files){return true;}, //this function runs before upload all start, can be good for validation
    	onSelect: 		function(files){},			//function that trigger after a file select has been made, paramter total files in the queue
    	onInit:			function(AU){},				//function that trigger on uploader initialization. Usefull if need to hide any button before uploader set up, without using css
    	language:		'auto',							//set regional language, default is english, avaiables: sq_AL, it_IT
    	uploadDir:		false,						//experimental feature, works on google chrome, for upload an entire folder content
    	removeOnSuccess:false,						//if true remove the file from the list after has been uploaded successfully
    	removeOnError:	false,						//if true remove the file from the list if it has errors during upload
    	bootstrap:		false						//tell if to use bootstrap for theming buttons
    };
    
	var methods =
	{
		init : function(options)
		{
    	    return this.each(function() 
    	    {
				var settings = $.extend({}, globalSettings, options);	
				
				//for avoiding two times call errors
				var $this 	= $(this).html('');
				var AU		= $this.data('AU');
				if( AU!==undefined && AU!==null)
				{
					return;
				}
				
				if(settings.language == 'auto')
				{
					var language = window.navigator.userLanguage || window.navigator.language;
					settings.language = language.replace('-', '_');
				}
				
				//load language
				load18(settings.language);
				
				$this.addClass('ax-uploader').data('author','http://www.albanx.com/');
				
				//create a iframe for standard uploads
				if($('#ax-main-frame').length==0) 	$('<iframe name="ax-main-frame" id="ax-main-frame" />').hide().appendTo('body');
				
				//setup a preview box for images
				if($('#ax-box').length==0)			$('<div id="ax-box"><div id="ax-box-fn"><span></span></div><img /><a id="ax-box-close" title="'+_('Close')+'"></a></div>').appendTo('body');//preview box
				if($('#ax-box-shadow').length==0)	$('<div id="ax-box-shadow"/>').appendTo('body');//preview shadow, overlay

   			    $('#ax-box-close, #ax-box-shadow').click(function(e){
	    			e.preventDefault();
	    			$('#ax-box').fadeOut(500);
	    			$('#ax-box-shadow').hide();
	    		});
   			    
   			    if(settings.bootstrap)
   			    {
   			    	$('#ax-box-close').addClass('btn btn-danger').html('<span class="ax-clear-icon ax-icon icon-remove-sign"></span>');
   			    }
   			    
   			    
   			    //generate an id if it has no one, so to point the uploader from flash
   			    var unique_id = 'AX_'+Math.floor(Math.random()*100001); 
   			    while($('#'+unique_id).length>0)
   			    {
   			    	unique_id = 'AX_'+Math.floor(Math.random()*100001); 
   			    }  			     
   			    this.id = (this.id) ? this.id: unique_id; 
   			 
				//Normalizze settings
				settings.allowExt 	= $.map(settings.allowExt, function(n, i){ return n.toLowerCase();  });
				
				//create the uploader object
				$this.data('AU', new AjaxUploader($this, settings));
    	    });
		},
		clear:function()
		{
			return this.each(function()
			{
				var $this = $(this);
				var AU = $this.data('AU');
				AU.clearQueue();
			});
		},
		start:function()
		{
			return this.each(function()
			{
				var $this = $(this);
				var AU = $this.data('AU');
				AU.uploadAll();
			});
		},
		addFlash:function(files)
		{
			var $this = $(this);
			var AU = $this.data('AU');
			AU.addFiles(files);
		},
		progressFlash: function(p, filepos)
		{
			var $this = $(this);
			var AU = $this.data('AU');
			AU.files[filepos].onProgress(p);
		},
		onFinishFlash: function(json, pos)
		{
			var $this = $(this);
			var AU = $this.data('AU');
			AU.uploading = false;
			try
			{
				var json_ret = jQuery.parseJSON(json);
				if(parseInt(json_ret.status) == -1)
				{
					throw json_ret.info;
				}
				else
				{
					AU.files[pos].onFinish(json_ret.name, json_ret.size, json_ret.status, json_ret.info);
				}
			}
			catch(err)
			{
				AU.files[pos].onError(err);
			}
			
			if(AU.upload_all)//upload next if upload all was pressed
			{
				var up_next = true;
				while(up_next)
				{
					pos++;
					if(AU.files[pos]!==undefined && AU.files[pos].status==0)
					{
						up_next = false;
						AU.files[pos].startUpload(AU.upload_all);
					}
					else if(AU.files[pos]!==undefined && AU.files[pos].status!=0)
					{
						up_next = true;
					}
					else
					{
						up_next = false;
					}
				}
			}
		},
		getUrl: function(name, size)
		{
			var $this 	= $(this);
			var AU 		= $this.data('AU');
			return AU.settings.url;
		},
		getParams: function(name, size)
		{
			var $this 	= $(this);
			var AU 		= $this.data('AU');
			var params	= AU.getParams(name, size, true);
			return params.join('&');
		},
		getAllowedExt: function(asArray)
		{
			var $this = $(this);
			var AU = $this.data('AU');
			var allowedExt = AU.settings.allowExt;
			
			return (asArray===true)?allowedExt:allowedExt.join('|');
		},
		getMaxFileNum: function(asArray)
		{
			var AU = $(this).data('AU');
			return AU.settings.maxFiles;
		},
		checkFile: function(name, size)
		{
			var AU = $(this).data('AU');
			return AU.checkFile(name, size) == '';
		},
		checkEnable: function(){
			return $(this).data('AU').settings.enable;
		},
		getFiles: function(){
			var AU = $(this).data('AU');
			return AU.files;
		},
		enable:function()
		{
			return this.each(function()
			{
				var AU = $(this).data('AU');
				AU.enable(true);
			});
		},
		disable:function()
		{
			return this.each(function()
			{
				var AU = $(this).data('AU');
				AU.enable(false);
			});
		},
		destroy : function()
		{
			return this.each(function()
			{
				var $this = $(this);
				var AU = $this.data('AU');//get ajax uploader object
				AU.clearQueue();//remove files in queue
				$this.removeData('AU').html('');//remove object and empty element
			});
		},
		option : function(option, value)
		{
			return this.each(function(){
				var AU = $(this).data('AU');
				return AU.options(option, value);
			});
		},
		debug: function(msg){
			console.log(msg);
		}
	};


	//jquery standard recomendation write of plugins
	$.fn.ajaxupload = function(method, options)
	{
		if(methods[method])
		{
			return methods[method].apply(this, Array.prototype.slice.call(arguments, 1));
		}
		else if(typeof method === 'object' || !method)
		{
			return methods.init.apply(this, arguments);
		}
		else
		{
			$.error('Method ' + method + ' does not exist on jQuery.AjaxUploader');
		}
	};

})(jQuery);
