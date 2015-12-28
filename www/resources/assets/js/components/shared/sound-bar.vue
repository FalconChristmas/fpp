<template>
    <div id="bars">
        <div class="bar"></div>
        <div class="bar"></div>
        <div class="bar"></div>
        <div class="bar"></div>
        <div class="bar"></div>
    </div>
</template>

<script>
    export default {};
</script>

<style lang="sass">
    // Some fancy bars for an eye-candy effect.
    // Credits: http://codepen.io/johnheiner/pen/JdRybK and http://codepen.io/jackrugile/pres/CkAbG
    @keyframes sound {
        0% {
           opacity: .15;
            height: 1px; 
        }
        100% {
            opacity: 1;       
            height: 13px;        
        }
    }

    #bars {
        width: 28px;
        height: 13px;
        position: relative;
        display: inline-block;

        .bar {
            background: #666;
            bottom: 1px;
            position: absolute;
            width: 3px;      
            animation: sound 0ms -800ms linear infinite alternate;
        }

        .bar:nth-child(1)  { left: 1px; background: #754fa0; animation-duration: 474ms; }
        .bar:nth-child(2)  { left: 5px; background: #09b7bf; animation-duration: 433ms; }
        .bar:nth-child(3)  { left: 9px; background: #90d36b; animation-duration: 407ms; }
        .bar:nth-child(4)  { left: 13px; background: #ed1b72; animation-duration: 458ms; }
        .bar:nth-child(5)  { left: 17px; background: #f2d40d; animation-duration: 400ms; }
    }
</style>
