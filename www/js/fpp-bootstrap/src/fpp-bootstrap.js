// import jQuery from 'jquery';
//import { gsap,Expo } from "gsap";
// import Swiper from 'swiper';
// import Dropzone from 'dropzone';

// window.$ = jQuery;
// $(()=>{
  
// })

import Swiper, { Navigation, Autoplay,Pagination  } from 'swiper';
Swiper.use([Navigation, Autoplay, Pagination ]);



window.addEventListener('DOMContentLoaded', (event) => {
    var mySwiper = new Swiper('.swiper-container', {
        loop:true,
        autoplay: {
            delay: 5000,
          },
          pagination: {
            el: '.swiper-pagination',
          },    

        navigation: {
          nextEl: '.swiper-button-next',
          prevEl: '.swiper-button-prev',
        },

      })
});