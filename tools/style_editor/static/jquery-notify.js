/**
 * Notification JS
 * Creates Notifications
 * @author Andrew Dodson
 */

(function($){

	// Relative Directory
	var dir = (function(){
					var script = document.getElementsByTagName("script");
					script = script[script.length-1];
					return ( script.src || script.getAttribute("src",4) ).replace(/[^\/]+$/, '');
				})();

	// default icon
	var star = dir + "star.ico";

	// Unique reference of the items in the
	var guid = [];


	$.extend({
		// Check for browser support
		notifyCheck : function(){
			// Check whether the current desktop supports notifications and if they are authorised,
			// 0 (yes they are supported and permission is granted),
			// 1 (yes they are supported, permission has not been granted),
			// -1 (Notifications are not supported)

			// IE9
			if(("external" in window) && ("msIsSiteMode" in window.external)){
				return window.external.msIsSiteMode()? 0 : 1;
			}
			else if("webkitNotifications" in window){
				return window.webkitNotifications.checkPermission() === 0 ? 0 : 1;
			}
			else {
				return -1;
			}
		},

		// Request browser adoption
		notifyRequest : function(cb){
			// Setup
			// triggers the authentication to create a notification
			cb = cb || function(){};

			// IE9
			if(("external" in window) && ("msIsSiteMode" in window.external)){
				if( !window.external.msIsSiteMode() ){
					window.external.msAddSiteMode();
					return true;
				}
				else {
					cb();
					return false;
				}
			}
			// If Chrome and not already enabled
			else if("webkitNotifications" in window && window.webkitNotifications.checkPermission() !== 0 ){
				return window.webkitNotifications.requestPermission(cb);
			}
			else {
				cb();
				return null;
			}
		},

		// Notify
		notify : function(icon, title, description, callback){

			// Create a notification
			if( typeof(icon) === 'string' ){
				createNotification( icon || star, title, description);
			}
			else if( typeof(icon) === 'object' ){

				var o = icon,
					path = o.path,
					interval = o.interval * 1e3,
					callback = o.callback,
					initial = o.initial;


				(function self(){

					$.getJSON("http://ajax.googleapis.com/ajax/services/feed/load?v=1.0&callback=?&q="+path, function(r){

						// loop through the rss items
						$.each( r.responseData.feed.entries, function(){

							var s = this.link;

							if(guid.indexOf(s)>-1){
								// dont do anything, ignore
								return;
							}

							// This is a new item
							// add to the list of GUID's
							guid.push(s);

							// If this is not the first pass them we need to trigger the notification
							if(!initial){
								$.notify( "", this.title, this.contentSnippet, callback );
							}
						});

						// is this the first pass?
						initial = false;
					});

					// call this again in one 60 seconds
					setTimeout(self,interval);
				})();
			}
		}
	});


	var a = [], int, i=0, n;

	function swaptitle(title){

		if(a.length===0){
			a = [document.title]
		}

		a.push(title);

		if(!int){
			int = setInterval(function(){

				// has document.title changed externally?
				if(a.indexOf(document.title) === -1 ){
					// update the default title
					a[0] = document.title;
				}

				document.title = a[++i%a.length];
			}, 1000);
		}
	}

	function addEvent(el,name,func){
		if(name.match(" ")){
			var a = name.split(' ');
			for(var i=0;i<a.length;i++){
				addEvent( el, a[i], func);
			}
		}
		if(el.addEventListener){
		    el.removeEventListener(name, func, false);
		    el.addEventListener(name, func, false);
		}
		else {
		    el.detachEvent('on'+name, func);
		    el.attachEvent('on'+name, func);
		}
	}

	addEvent(window, "focus scroll click", function(){

		// if a webkit Notification is open, kill it
		if(n){
			n.cancel();
		}

		// if an IE overlay is present, kill it
		if("external" in window && "msSiteModeClearIconOverlay" in window.external ){
			window.external.msSiteModeClearIconOverlay();
		}

		// dont do any more if we haven't got anything open
		if(a.length===0){
			return;
		}
		clearInterval(int);

		int = false;
		document.title = a[0];
		a = [];
		i = 0;
	});

	function createNotification(icon, title, description, ttl){
		// Create a notification
		// @icon string
		// @title string
		// @description string
		// @ttl string

		//
		// Swap document.title
		//
		swaptitle(title);

		//
		// Create Desktop Notifications
		//
		if(("external" in window) && ("msIsSiteMode" in window.external)){
			if(window.external.msIsSiteMode()){
				window.external.msSiteModeActivate();

				if(icon){
					window.external.msSiteModeSetIconOverlay(icon, title);
				}

				return true;
			}
			return false;
		}
		else if("webkitNotifications" in window){
			if(window.webkitNotifications.checkPermission() === 0){
				n = window.webkitNotifications.createNotification(icon, title, description )
				n.show();
				n.onclick = function(){
					// redirect the user back to the page
					window.focus();
					setTimeout( function(){ n.cancel(); }, 1000);
				};
				if(ttl>0){
					setTimeout( function(){ n.cancel(); }, ttl);
				}
				return n;
			}
			return false;
		}
		else if( "mozNotification" in window.navigator ){
			var m = window.navigator.mozNotification.createNotification( title, description, icon );
			m.show();
			return true;
		}
		else {
			return null;
		}
	};

})(jQuery);