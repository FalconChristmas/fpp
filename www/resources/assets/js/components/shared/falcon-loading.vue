<template>
    <div id="load">
        <span class="letter">F</span>
        <span class="letter">A</span>
        <span class="letter">L</span>
        <span class="letter">C</span>
        <span class="letter">O</span>
        <span class="letter">N</span>
        <span class="letter">P</span>
        <span class="letter">L</span>
        <span class="letter">A</span>
        <span class="letter">Y</span>
        <span class="letter">E</span>
        <span class="letter">R</span>
      
    </div>
</template>

<script>
    export default {};
</script>

<style lang="sass">
    // Some fancy bars for an eye-candy effect.
    // Credits: http://codepen.io/johnheiner/pen/JdRybK and http://codepen.io/jackrugile/pres/CkAbG
    @keyframes grow {
        0% {
           opacity: .25;
           transform: scale(.9);
        }
        33% {
            opacity: 1;       
            transform: scale(1);      
        }
        66% {
           opacity: .25;
           transform: scale(.9);     
        }
        100% {
           opacity: .25;
           transform: scale(.9);     
        }
    }

    #load {
        // width: 28px;
        // height: 13px;
        position: relative;
        display: inline-block;
        text-align: center;

        .letter {
            display: inline-block;
            margin: 0 2px;    
            color: white;
            opacity: .25;
            transform: scale(.9);
            animation: grow 2.5s ease-in infinite;
        }        
               

        .letter:nth-child(1) { animation-delay: .1s; }
        .letter:nth-child(2) { animation-delay: .15s; }
        .letter:nth-child(3) { animation-delay: .2s; }
        .letter:nth-child(4) { animation-delay: .25s; }
        .letter:nth-child(5) { animation-delay: .3s; }
        .letter:nth-child(6) { animation-delay: .35s; }
        .letter:nth-child(7) { margin-left: 8px; animation-delay: .4s; }
        .letter:nth-child(8) { animation-delay: .45s; }
        .letter:nth-child(9) { animation-delay: .5s; }
        .letter:nth-child(10) { animation-delay: .55s; }
        .letter:nth-child(11) { animation-delay: .6s; }
        .letter:nth-child(12) { animation-delay: .65s; }
        
       
        
       
    }
</style>
